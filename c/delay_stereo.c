// -------------------------------------------------------------------
// delay_stereo.c
//
// Free software by Philipp Müller based on the work of Richard
// W.E. Furse. Do with as you will. No warranty.
//
// This LADSPA plugin provides a simple stereo delay line implemented
// in C. There is a fixed maximum delay length and no feedback is
// provided.
//
// This file has poor memory protection. Failures during malloc() will
// not recover nicely.
// -------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

// -------------------------------------------------------------------

// Include headers shipped in this repo.
#include "ladspa.h"
#include "utils.h"

// -------------------------------------------------------------------

// The maximum delay valid for the delay line (in seconds).
#define MAX_DELAY 5

// -------------------------------------------------------------------

// The port numbers for the plugin
#define SDL_DELAY_LENGTH_LEFT  0
#define SDL_DELAY_LENGTH_RIGHT 1
#define SDL_DRY_WET_LEFT       2
#define SDL_DRY_WET_RIGHT      3
#define SDL_INPUT_LEFT         4
#define SDL_INPUT_RIGHT        5
#define SDL_OUTPUT_LEFT        6
#define SDL_OUTPUT_RIGHT       7

// -------------------------------------------------------------------

// A couple of helper macros.
#define LIMIT_BETWEEN_0_AND_1(x)		\
  (((x) < 0) ? 0 : (((x) > 1) ? 1 : (x)))
#define LIMIT_BETWEEN_0_AND_MAX_DELAY(x)			\
  (((x) < 0) ? 0 : (((x) > MAX_DELAY) ? MAX_DELAY : (x)))

// -------------------------------------------------------------------

// Instance data for the simple delay line plugin.
typedef struct {

  LADSPA_Data m_fSampleRate;

  // Buffers which will contain the information of the left and right
  // channel.
  LADSPA_Data* m_pfBufferLeft;
  LADSPA_Data* m_pfBufferRight;

  // Buffer size, a power of two.
  unsigned long m_lBufferSize;

  // Write pointer in buffers. Both will share the some pointer.
  unsigned long m_lWritePointer;

  // Ports:
  // ------
  // Delay controls, in seconds. Accepted between 0 and 1 (only 1 sec
  // of buffer is allocated by this crude delay line).
  LADSPA_Data* m_pfDelayLeft;
  LADSPA_Data* m_pfDelayRight;

  // Dry/wet controls. 0 for entirely dry, 1 for entirely wet.
  LADSPA_Data* m_pfDryWetLeft;
  LADSPA_Data* m_pfDryWetRight;

  // Input audio ports data location.
  LADSPA_Data* m_pfInputLeft;
  LADSPA_Data* m_pfInputRight;

  // Output audio ports data location.
  LADSPA_Data* m_pfOutputLeft;
  LADSPA_Data* m_pfOutputRight;

} SimpleDelayLine;

// -------------------------------------------------------------------

// Construct a new plugin instance.
static LADSPA_Handle 
instantiateSimpleDelayLine(const LADSPA_Descriptor*  Descriptor,
			   unsigned long             SampleRate) {

  unsigned long lMinimumBufferSize;
  SimpleDelayLine* psDelayLine;
  
  // -----------------------------------------------------------------
  
  // Create an instance of the delay line.
  psDelayLine 
    = (SimpleDelayLine*)malloc(sizeof(SimpleDelayLine));

  if (psDelayLine == NULL) 
    return NULL;

  // -----------------------------------------------------------------
    
  psDelayLine->m_fSampleRate = (LADSPA_Data)SampleRate;

  // -----------------------------------------------------------------
  
  // Buffer size is a power of two bigger than max delay time.
  lMinimumBufferSize = (unsigned long)((LADSPA_Data)SampleRate * MAX_DELAY);
  psDelayLine->m_lBufferSize = 1;
  while (psDelayLine->m_lBufferSize < lMinimumBufferSize) {
    psDelayLine->m_lBufferSize <<= 1;
  }
  
  // -----------------------------------------------------------------
  
  psDelayLine->m_pfBufferLeft
    = (LADSPA_Data*)calloc(psDelayLine->m_lBufferSize, sizeof(LADSPA_Data));
  psDelayLine->m_pfBufferRight
    = (LADSPA_Data*)calloc(psDelayLine->m_lBufferSize, sizeof(LADSPA_Data));
  if (psDelayLine->m_pfBufferLeft == NULL || 
      psDelayLine->m_pfBufferRight == NULL) {
    free(psDelayLine);
    return NULL;
  }

  // -----------------------------------------------------------------
  
  psDelayLine->m_lWritePointer = 0;
  
  // -----------------------------------------------------------------
  
  return psDelayLine;
}

// -------------------------------------------------------------------

// Initialise and activate a plugin instance.
static void activateSimpleDelayLine(LADSPA_Handle Instance) {

  SimpleDelayLine* psSimpleDelayLine;
  psSimpleDelayLine = (SimpleDelayLine*)Instance;

  // -----------------------------------------------------------------

  // Need to reset the delay history in this function rather than
  // instantiate() in case deactivate() followed by activate() have
  // been called to reinitialise a delay line.
  memset(psSimpleDelayLine->m_pfBufferLeft, 
	 0, 
	 sizeof(LADSPA_Data) * psSimpleDelayLine->m_lBufferSize);
  memset(psSimpleDelayLine->m_pfBufferRight, 
	 0, 
	 sizeof(LADSPA_Data) * psSimpleDelayLine->m_lBufferSize);
}

// -------------------------------------------------------------------

// Connect a port to a data location.
static void 
connectPortToSimpleDelayLine(LADSPA_Handle Instance,
			     unsigned long Port,
			     LADSPA_Data* DataLocation) {

  SimpleDelayLine* psSimpleDelayLine;

  // -----------------------------------------------------------------
  
  psSimpleDelayLine = (SimpleDelayLine*)Instance;
  
  // -----------------------------------------------------------------
  
  switch (Port) {
  case SDL_DELAY_LENGTH_LEFT:
    psSimpleDelayLine->m_pfDelayLeft = DataLocation;
    break;
  case SDL_DELAY_LENGTH_RIGHT:
    psSimpleDelayLine->m_pfDelayRight = DataLocation;
    break;
  case SDL_DRY_WET_LEFT:
    psSimpleDelayLine->m_pfDryWetLeft = DataLocation;
    break;
  case SDL_DRY_WET_RIGHT:
    psSimpleDelayLine->m_pfDryWetRight = DataLocation;
    break;
  case SDL_INPUT_LEFT:
    psSimpleDelayLine->m_pfInputLeft = DataLocation;
    break;
  case SDL_INPUT_RIGHT:
    psSimpleDelayLine->m_pfInputRight = DataLocation;
    break;
  case SDL_OUTPUT_LEFT:
    psSimpleDelayLine->m_pfOutputLeft = DataLocation;
    break;
  case SDL_OUTPUT_RIGHT:
    psSimpleDelayLine->m_pfOutputRight = DataLocation;
    break;
  }
}

// -------------------------------------------------------------------

// Run a delay line instance for a block of SampleCount samples.
static void runSimpleDelayLine(LADSPA_Handle Instance,
			       unsigned long SampleCount) {
  
  LADSPA_Data* pfBufferLeft;
  LADSPA_Data* pfBufferRight;
  LADSPA_Data* pfInputLeft;
  LADSPA_Data* pfInputRight;
  LADSPA_Data* pfOutputLeft;
  LADSPA_Data* pfOutputRight;
  LADSPA_Data fDryLeft;
  LADSPA_Data fDryRight;
  LADSPA_Data fInputSampleLeft;
  LADSPA_Data fInputSampleRight;
  LADSPA_Data fWetLeft;
  LADSPA_Data fWetRight;
  SimpleDelayLine* psSimpleDelayLine;
  unsigned long lBufferReadOffsetLeft;
  unsigned long lBufferReadOffsetRight;
  unsigned long lBufferSizeMinusOne;
  unsigned long lBufferWriteOffset;
  unsigned long lDelayLeft;
  unsigned long lDelayRight;
  unsigned long lSampleIndex;

  // -----------------------------------------------------------------
  
  psSimpleDelayLine = (SimpleDelayLine*)Instance;

  // -----------------------------------------------------------------
  
  lBufferSizeMinusOne = psSimpleDelayLine->m_lBufferSize - 1;
  
  // -----------------------------------------------------------------
  
  lDelayLeft = (unsigned long)
    (LIMIT_BETWEEN_0_AND_MAX_DELAY(*(psSimpleDelayLine->m_pfDelayLeft)) 
     * psSimpleDelayLine->m_fSampleRate);
  lDelayRight = (unsigned long)
    (LIMIT_BETWEEN_0_AND_MAX_DELAY(*(psSimpleDelayLine->m_pfDelayRight))
     * psSimpleDelayLine->m_fSampleRate);

  // -----------------------------------------------------------------
  
  pfInputLeft = psSimpleDelayLine->m_pfInputLeft;
  pfInputRight = psSimpleDelayLine->m_pfInputRight;
  
  // -----------------------------------------------------------------
  
  pfOutputLeft = psSimpleDelayLine->m_pfOutputLeft;
  pfOutputRight = psSimpleDelayLine->m_pfOutputRight;
  
  // -----------------------------------------------------------------
  
  pfBufferLeft = psSimpleDelayLine->m_pfBufferLeft;
  pfBufferRight = psSimpleDelayLine->m_pfBufferRight;
  
  // -----------------------------------------------------------------
  
  lBufferWriteOffset = psSimpleDelayLine->m_lWritePointer;
  
  // -----------------------------------------------------------------
  
  lBufferReadOffsetLeft
    = lBufferWriteOffset + psSimpleDelayLine->m_lBufferSize - lDelayLeft;
  lBufferReadOffsetRight
    = lBufferWriteOffset + psSimpleDelayLine->m_lBufferSize - lDelayRight;
  
  // -----------------------------------------------------------------
  
  fWetLeft = LIMIT_BETWEEN_0_AND_1(*(psSimpleDelayLine->m_pfDryWetLeft));
  fWetRight = LIMIT_BETWEEN_0_AND_1(*(psSimpleDelayLine->m_pfDryWetRight));
  
  // -----------------------------------------------------------------
  
  fDryLeft = 1 - fWetLeft;
  fDryRight = 1 - fWetRight;

  // -----------------------------------------------------------------
  
  for (lSampleIndex = 0; lSampleIndex < SampleCount; lSampleIndex++) {
    fInputSampleLeft = *(pfInputLeft++);
    fInputSampleRight = *(pfInputRight++);
    
    // ---------------------------------------------------------------
    
    *(pfOutputLeft++) = (fDryLeft * fInputSampleLeft
			 + fWetLeft * pfBufferLeft[((lSampleIndex + lBufferReadOffsetLeft)
						    & lBufferSizeMinusOne)]);
    *(pfOutputRight++) = (fDryRight * fInputSampleRight
			  + fWetRight * pfBufferRight[((lSampleIndex + lBufferReadOffsetRight)
						       & lBufferSizeMinusOne)]);
    
    // ---------------------------------------------------------------
    
    pfBufferLeft[((lSampleIndex + lBufferWriteOffset)
		  & lBufferSizeMinusOne)] = fInputSampleLeft;
    pfBufferRight[((lSampleIndex + lBufferWriteOffset)
		   & lBufferSizeMinusOne)] = fInputSampleRight;
  }

  // -----------------------------------------------------------------
  
  psSimpleDelayLine->m_lWritePointer
    = ((psSimpleDelayLine->m_lWritePointer + SampleCount)
       & lBufferSizeMinusOne);
}

// -------------------------------------------------------------------

// Throw away a simple delay line.
static void cleanupSimpleDelayLine(LADSPA_Handle Instance) {

  SimpleDelayLine* psSimpleDelayLine;

  // -----------------------------------------------------------------
  
  psSimpleDelayLine = (SimpleDelayLine*)Instance;

  // -----------------------------------------------------------------
  
  free(psSimpleDelayLine->m_pfBufferLeft);
  free(psSimpleDelayLine->m_pfBufferRight);
  free(psSimpleDelayLine);
}

// -------------------------------------------------------------------

static LADSPA_Descriptor* g_psDescriptor = NULL;

// -------------------------------------------------------------------

// Called automatically when the plugin library is first loaded.
ON_LOAD_ROUTINE {

  char** pcPortNames;
  LADSPA_PortDescriptor* piPortDescriptors;
  LADSPA_PortRangeHint* psPortRangeHints;

  // -----------------------------------------------------------------
  
  g_psDescriptor
    = (LADSPA_Descriptor*)malloc(sizeof(LADSPA_Descriptor));
  
  // -----------------------------------------------------------------
  
  if (g_psDescriptor) {
    g_psDescriptor->UniqueID
      = 399;
    g_psDescriptor->Label
      = strdup("c_delay_5s_stereo");
    g_psDescriptor->Properties
      = LADSPA_PROPERTY_HARD_RT_CAPABLE;
    g_psDescriptor->Name 
      = strdup("Simple Stereo Delay Line");
    g_psDescriptor->Maker
      = strdup("Richard Furse (LADSPA example plugins)");
    g_psDescriptor->Copyright
      = strdup("None");
    g_psDescriptor->PortCount 
      = 8;
    
    // ---------------------------------------------------------------
    
    piPortDescriptors
      = (LADSPA_PortDescriptor*)calloc(8, sizeof(LADSPA_PortDescriptor));
    g_psDescriptor->PortDescriptors 
      = (const LADSPA_PortDescriptor*)piPortDescriptors;
    
    // ---------------------------------------------------------------
    
    piPortDescriptors[SDL_DELAY_LENGTH_LEFT]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_DELAY_LENGTH_RIGHT]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_DRY_WET_LEFT]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_DRY_WET_RIGHT]
      = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
    piPortDescriptors[SDL_INPUT_LEFT]
      = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
    piPortDescriptors[SDL_INPUT_RIGHT]
      = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
    piPortDescriptors[SDL_OUTPUT_LEFT]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
    piPortDescriptors[SDL_OUTPUT_RIGHT]
      = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
    
    // ---------------------------------------------------------------
    
    pcPortNames
      = (char **)calloc(8, sizeof(char *));
    g_psDescriptor->PortNames
      = (const char **)pcPortNames;
    
    // ---------------------------------------------------------------
    
    pcPortNames[SDL_DELAY_LENGTH_LEFT]
      = strdup("Delay (Seconds) (Left)");
    pcPortNames[SDL_DELAY_LENGTH_RIGHT]
      = strdup("Delay (Seconds) (Right)");
    pcPortNames[SDL_DRY_WET_LEFT] 
      = strdup("Dry/Wet Balance (Left)");
    pcPortNames[SDL_DRY_WET_RIGHT] 
      = strdup("Dry/Wet Balance (Right)");
    pcPortNames[SDL_INPUT_LEFT] 
      = strdup("Input (Left)");
    pcPortNames[SDL_INPUT_RIGHT] 
      = strdup("Input (Right)");
    pcPortNames[SDL_OUTPUT_LEFT]
      = strdup("Output (Left)");
    pcPortNames[SDL_OUTPUT_RIGHT]
      = strdup("Output (Right)");

    // ---------------------------------------------------------------
        
    psPortRangeHints = ((LADSPA_PortRangeHint*)
			calloc(8, sizeof(LADSPA_PortRangeHint)));
    g_psDescriptor->PortRangeHints
      = (const LADSPA_PortRangeHint*)psPortRangeHints;

    // ---------------------------------------------------------------
        
    psPortRangeHints[SDL_DELAY_LENGTH_LEFT].HintDescriptor
      = (LADSPA_HINT_BOUNDED_BELOW 
	 | LADSPA_HINT_BOUNDED_ABOVE
	 | LADSPA_HINT_DEFAULT_1);
    psPortRangeHints[SDL_DELAY_LENGTH_LEFT].LowerBound 
      = 0;
    psPortRangeHints[SDL_DELAY_LENGTH_LEFT].UpperBound
      = (LADSPA_Data)MAX_DELAY;
    psPortRangeHints[SDL_DELAY_LENGTH_RIGHT].HintDescriptor
      = psPortRangeHints[SDL_DELAY_LENGTH_LEFT].HintDescriptor;
    psPortRangeHints[SDL_DELAY_LENGTH_RIGHT].LowerBound
      = psPortRangeHints[SDL_DELAY_LENGTH_LEFT].LowerBound;
    psPortRangeHints[SDL_DELAY_LENGTH_RIGHT].UpperBound
      = psPortRangeHints[SDL_DELAY_LENGTH_LEFT].UpperBound;
    
    // ---------------------------------------------------------------
    
    psPortRangeHints[SDL_DRY_WET_LEFT].HintDescriptor
      = (LADSPA_HINT_BOUNDED_BELOW 
	 | LADSPA_HINT_BOUNDED_ABOVE
	 | LADSPA_HINT_DEFAULT_MIDDLE);
    psPortRangeHints[SDL_DRY_WET_LEFT].LowerBound 
      = 0;
    psPortRangeHints[SDL_DRY_WET_LEFT].UpperBound
      = 1;
    psPortRangeHints[SDL_DRY_WET_RIGHT].HintDescriptor
      = psPortRangeHints[SDL_DRY_WET_LEFT].HintDescriptor;
    psPortRangeHints[SDL_DRY_WET_RIGHT].LowerBound
      = psPortRangeHints[SDL_DRY_WET_LEFT].LowerBound;
    psPortRangeHints[SDL_DRY_WET_RIGHT].UpperBound
      = psPortRangeHints[SDL_DRY_WET_LEFT].UpperBound;
    
    // ---------------------------------------------------------------
    
    psPortRangeHints[SDL_INPUT_LEFT].HintDescriptor
      = 0;
    psPortRangeHints[SDL_INPUT_RIGHT].UpperBound
      = psPortRangeHints[SDL_INPUT_LEFT].UpperBound;
    
    // ---------------------------------------------------------------
    
    psPortRangeHints[SDL_OUTPUT_LEFT].HintDescriptor
      = 0;
    psPortRangeHints[SDL_OUTPUT_RIGHT].UpperBound
      = psPortRangeHints[SDL_OUTPUT_LEFT].UpperBound;

    // ---------------------------------------------------------------
        
    g_psDescriptor->instantiate
      = instantiateSimpleDelayLine;
    g_psDescriptor->connect_port 
      = connectPortToSimpleDelayLine;
    g_psDescriptor->activate
      = activateSimpleDelayLine;
    g_psDescriptor->run 
      = runSimpleDelayLine;
    g_psDescriptor->run_adding
      = NULL;
    g_psDescriptor->set_run_adding_gain
      = NULL;
    g_psDescriptor->deactivate
      = NULL;
    g_psDescriptor->cleanup
      = cleanupSimpleDelayLine;
  }
}

// -------------------------------------------------------------------

// Called automatically when the library is unloaded.
ON_UNLOAD_ROUTINE {
  long lIndex;
  
  // -----------------------------------------------------------------
  
  if (g_psDescriptor) {
    free((char*)g_psDescriptor->Label);
    free((char*)g_psDescriptor->Name);
    free((char*)g_psDescriptor->Maker);
    free((char*)g_psDescriptor->Copyright);
    
    // ---------------------------------------------------------------
    
    free((LADSPA_PortDescriptor*)g_psDescriptor->PortDescriptors);
    
    // ---------------------------------------------------------------
    
    for (lIndex = 0; lIndex < g_psDescriptor->PortCount; lIndex++) {
      free((char*)(g_psDescriptor->PortNames[lIndex]));
    }
    
    // ---------------------------------------------------------------
    
    free((char**)g_psDescriptor->PortNames);
    
    // ---------------------------------------------------------------
    
    free((LADSPA_PortRangeHint*)g_psDescriptor->PortRangeHints);
    
    // ---------------------------------------------------------------
    
    free(g_psDescriptor);
  }
}

// -------------------------------------------------------------------

// Return a descriptor of the requested plugin type. Only one plugin
// type is available in this library.
const LADSPA_Descriptor* ladspa_descriptor(unsigned long Index) {
  if (Index == 0)
    return g_psDescriptor;
  else
    return NULL;
}

// -------------------------------------------------------------------
