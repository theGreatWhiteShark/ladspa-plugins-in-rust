extern crate ladspa;

use ladspa::{PluginDescriptor, PortDescriptor, Port, DefaultValue,
	     Data, Plugin, PortConnection};
use std::default::Default;

// -------------------------------------------------------------------

const MAX_DELAY: Data = 5.0;

// -------------------------------------------------------------------

struct Delay {
    sample_rate: Data,
    buf: Vec<(Data, Data)>,
    buf_idx: usize,
}

// -------------------------------------------------------------------

fn new_delay(_: &PluginDescriptor, sample_rate: u64) -> Box<Plugin + Send> {
    Box::new(Delay {
	sample_rate: sample_rate as Data,
	buf: Vec::new(),
	buf_idx: 0,
    })
}

// -------------------------------------------------------------------

impl Plugin for Delay {
    
    // ---------------------------------------------------------------

    fn activate(&mut self) {
	self.buf.clear();
	self.buf.resize((self.sample_rate * MAX_DELAY * 1.0) as usize + 1,
			(0.0, 0.0));
	self.buf_idx = 0;
    }
    
    // ---------------------------------------------------------------
    
    fn run<'a>(&mut self, sample_count: usize, ports: &[&'a PortConnection<'a>]) {
	let input = (ports[0].unwrap_audio(), ports[1].unwrap_audio());
	let mut output = (ports[2].unwrap_audio_mut(), ports[3].unwrap_audio_mut());
	
	// -----------------------------------------------------------

	let delay = ((*ports[4].unwrap_control() * self.sample_rate) as usize,
		     (*ports[5].unwrap_control() * self.sample_rate) as usize);
	
	// -----------------------------------------------------------

	let dry_wet = (*ports[6].unwrap_control(), *ports[7].unwrap_control());
	
	// -----------------------------------------------------------

	let buffer_read_idx = (self.buf_idx + self.buf.len() - delay.0,
			       self.buf_idx + self.buf.len() - delay.1);
	let buf_len = self.buf.len();
	
	// -----------------------------------------------------------

	for ii in 0..sample_count {
	    
	    // -------------------------------------------------------
	    // Read in a sample
	    let input_sample = (input.0[ii], input.1[ii]);
	    
	    // -------------------------------------------------------
	    // Calculate the stereo output.
	    output.0[ii] = input_sample.0 * (1.0 - dry_wet.0) +
		dry_wet.0 * self.buf[(buffer_read_idx.0 + ii) % buf_len].0;
	    output.1[ii] = input_sample.1 * (1.0 - dry_wet.1) +
		dry_wet.1 * self.buf[(buffer_read_idx.1 + ii) % buf_len].1;
	    
	    // -------------------------------------------------------
	    // Store the sample in the buffer.
	    self.buf[(ii + self.buf_idx) % buf_len] = input_sample;
		
	    // -------------------------------------------------------

	}
	
	// -----------------------------------------------------------
	// Update the buffer index.
	self.buf_idx += sample_count;
	self.buf_idx %= buf_len;
	
	// -----------------------------------------------------------
	
    }

}

// -------------------------------------------------------------------

#[no_mangle]
pub fn get_ladspa_descriptor(index: u64) -> Option<PluginDescriptor> {
    match index {
	0 => {
	    Some(PluginDescriptor {
		unique_id: 400,
		label: "rust_delay_5s_stereo",
		properties: ladspa::PROP_NONE,
		name: "LADSPA Stereo Delay example in Rust",
		maker: "thegreatwhiteshark",
		copyright: "None",
		ports: vec![
		    Port {
			name: "Left Audio In",
			desc: PortDescriptor::AudioInput,
			..Default::default()
		    },
		    Port {
			name: "Right Audio In",
			desc: PortDescriptor::AudioInput,
			..Default::default()
		    },
		    Port {
			name: "Left Audio Out",
			desc: PortDescriptor::AudioOutput,
			..Default::default()
		    },
		    Port {
			name: "Right Audio Out",
			desc: PortDescriptor::AudioOutput,
			..Default::default()
		    },
		    Port {
			name: "Left Delay (seconds)",
			desc: PortDescriptor::ControlInput,
			hint: None,
			default: Some(DefaultValue::Value1),
			lower_bound: Some(0.0),
			upper_bound: Some(MAX_DELAY),
		    },
		    Port {
			name: "Right Delay (seconds)",
			desc: PortDescriptor::ControlInput,
			hint: None,
			default: Some(DefaultValue::Value1),
			lower_bound: Some(0.0),
			upper_bound: Some(MAX_DELAY),
		    },
		    Port {
			name: "Left Dry/Wet",
			desc: PortDescriptor::ControlInput,
			hint: None,
			default: Some(DefaultValue::Middle),
			lower_bound: Some(0.0),
			upper_bound: Some(1.0),
		    },
		    Port {
			name: "Right Dry/Wet",
			desc: PortDescriptor::ControlInput,
			hint: None,
			default: Some(DefaultValue::Middle),
			lower_bound: Some(0.0),
			upper_bound: Some(1.0),
		    },
		],
		new: new_delay,

	    })
	},
	
	// -----------------------------------------------------------
	
	_ => None
    }
}

// -------------------------------------------------------------------
