//-----------------------------------------Definitions and Settings-------------------------------------------------
#include <FastLED.h>
#define NUM_LEDS 60
#define NUM_STRIPS 1
#define DATA_PIN 1
#define NUM_CHANNELS 12
#define ADSR_FRAME_TIME 5 //in milliseconds
#define NUM_ADSR_CHANNELS 8
#define MAX_ADSR_POLY 12
#define GRADIENT_FRAME_TIME 30 //in milliseconds
#define NUM_GRADIENT_CHANNELS 4
#define MAX_GRADIENT_POLY 4
#define noteBrightConstant 125   //max brightness of an individual note ---- headroom for summing

//----------------------------------------------MIDI Variables------------------------------------------------------
//ADSR-Specific----------
byte adsrCurrentNotes[NUM_ADSR_CHANNELS][MAX_ADSR_POLY];
byte adsrCurrentVelocities[NUM_ADSR_CHANNELS][MAX_ADSR_POLY];
bool adsrNoteActive[NUM_ADSR_CHANNELS][MAX_ADSR_POLY];
int adsrLastNoteWritten[NUM_ADSR_CHANNELS];
//GRADIENT-Specific------
byte gradientCurrentNotes[NUM_GRADIENT_CHANNELS][MAX_GRADIENT_POLY];
bool gradientChannelActive[NUM_GRADIENT_CHANNELS];
bool gradientNoteActive[NUM_GRADIENT_CHANNELS][MAX_GRADIENT_POLY];
bool gradientHasChanged[NUM_GRADIENT_CHANNELS];

//-----------------------------------------"Synth"/ADSR Variables---------------------------------------------------
unsigned int adsrCurrentFrame[NUM_ADSR_CHANNELS][MAX_ADSR_POLY];
bool atkOn[NUM_ADSR_CHANNELS][MAX_ADSR_POLY];
bool holdOn[NUM_ADSR_CHANNELS][MAX_ADSR_POLY];
bool decayOn[NUM_ADSR_CHANNELS][MAX_ADSR_POLY];
bool susOn[NUM_ADSR_CHANNELS][MAX_ADSR_POLY];
bool releaseOn[NUM_ADSR_CHANNELS][MAX_ADSR_POLY];
//ADSR Timings----------
//--Defaults
unsigned int defaultAtkFrames[NUM_ADSR_CHANNELS] =     {0, 20, 4, 150, 10, 50, 100, 200};
unsigned int defaultHoldFrames[NUM_ADSR_CHANNELS] =    {10, 10, 20, 40, 0, 0, 0, 0};
unsigned int defaultDecayFrames[NUM_ADSR_CHANNELS] =   {10, 500, 400, 300, 0, 0, 0, 200};
float defaultSustainValue[NUM_ADSR_CHANNELS] =         {0, 0.5, 0.3, 0.75, 1, 1, 1, 0.5};
unsigned int defaultReleaseFrames[NUM_ADSR_CHANNELS] = {0, 50, 40, 100, 10, 50, 100, 200};
//--Variables
unsigned int atkFrames[NUM_ADSR_CHANNELS];
unsigned int holdFrames[NUM_ADSR_CHANNELS];
unsigned int decayFrames[NUM_ADSR_CHANNELS];
float sustainValue[NUM_ADSR_CHANNELS];
unsigned int releaseFrames[NUM_ADSR_CHANNELS];
float adsrBrightnessCVs[NUM_ADSR_CHANNELS][MAX_ADSR_POLY];
float adsrBrightnessCVsAtRelease[NUM_ADSR_CHANNELS][MAX_ADSR_POLY];


//---------------------------------------------LED Variables--------------------------------------------------------
//ADSR-Specific------------------------------
CHSV adsrNoteHSV[NUM_ADSR_CHANNELS][MAX_ADSR_POLY];     //keep it fancy. Storage for HSV note values
CRGB adsrNoteRGBData[NUM_ADSR_CHANNELS][MAX_ADSR_POLY]; //push active notes to here for summing, else push black
//Gradient-Specific-------------------------
CHSV gradientPointValues[NUM_GRADIENT_CHANNELS][MAX_GRADIENT_POLY];
CRGB currentGradientRGB[NUM_GRADIENT_CHANNELS][NUM_LEDS];
CRGB targetGradientRGB[NUM_GRADIENT_CHANNELS][NUM_LEDS];
int numberActiveGradientPoints[NUM_GRADIENT_CHANNELS];
int activeGradientIndexes[NUM_GRADIENT_CHANNELS][MAX_GRADIENT_POLY];
byte blendAmounts[NUM_GRADIENT_CHANNELS];
byte defaultBlendAmounts[NUM_GRADIENT_CHANNELS] = {150, 75, 10, 5};   //fractions of 255
//Channel-Global-----------------------------
byte channelSaturation[NUM_CHANNELS] = {255,255,255,255,
                                        255,255,255,255,
                                        255,255,255,255}; //default to full color -- mod wheel
CRGB channelRGBData[NUM_CHANNELS][NUM_LEDS];  //each channel has its own pixel set. Sum to main or breakout display

CRGB leds[NUM_LEDS];                      //Main led array

//==================================================================================================================
//------------------------------------------ADSR/Color Functions----------------------------------------------------
void HandleADSR(int cIndex, int nIndex)
{
  adsrCurrentFrame[cIndex][nIndex]++;
  float decayAmount = 1.0-sustainValue[cIndex];
  float releaseAmount = adsrBrightnessCVsAtRelease[cIndex][nIndex];
//ATK-------  
  if(atkOn[cIndex][nIndex])
  {
    adsrBrightnessCVs[cIndex][nIndex] = (float)adsrCurrentFrame[cIndex][nIndex]/(float)atkFrames[cIndex];
    
    if(adsrCurrentFrame[cIndex][nIndex]>=atkFrames[cIndex])
    {
      atkOn[cIndex][nIndex] = false;
      holdOn[cIndex][nIndex] = true;
      adsrCurrentFrame[cIndex][nIndex] = 0;
    }
    return;
  }
//HOLD------
  else if(holdOn[cIndex][nIndex])
  {
    adsrBrightnessCVs[cIndex][nIndex] = 1.0;
    
    if(adsrCurrentFrame[cIndex][nIndex]>=holdFrames[cIndex])
    {
      holdOn[cIndex][nIndex] = false;
      decayOn[cIndex][nIndex] = true;
      adsrCurrentFrame[cIndex][nIndex] = 0;
    }
    return;
  }
//DECAY-----
  else if(decayOn[cIndex][nIndex])
  {
    //---
    adsrBrightnessCVs[cIndex][nIndex] 
    = 
    1.0-(((float)adsrCurrentFrame[cIndex][nIndex]/(float)decayFrames[cIndex])*decayAmount);
    //---
    
    if(adsrCurrentFrame[cIndex][nIndex]>=decayFrames[cIndex])
    {
      decayOn[cIndex][nIndex] = false;
      susOn[cIndex][nIndex] = true;
      adsrCurrentFrame[cIndex][nIndex] = 0;
    }
    return;
  }
//SUSTAIN--- 
  else if(susOn[cIndex][nIndex]&&adsrCurrentFrame[cIndex][nIndex]<2) //only need to set this once
  {
      adsrBrightnessCVs[cIndex][nIndex] = sustainValue[cIndex];
      return;
  }
//RELEASE---
  else if(releaseOn[cIndex][nIndex])
  {
    //---
    adsrBrightnessCVs[cIndex][nIndex] 
    = 
    releaseAmount-(releaseAmount*((float)adsrCurrentFrame[cIndex][nIndex]/(float)releaseFrames[cIndex]));
    //---
    
    if(adsrCurrentFrame[cIndex][nIndex]>=releaseFrames[cIndex])
    {
      adsrNoteActive[cIndex][nIndex] = false;
      adsrNoteHSV[cIndex][nIndex].v = 0;
      adsrCurrentNotes[cIndex][nIndex] = 0;
    }
    return;
  }
//?---------
  else{return;}
}

//------------------------------------------------------------------------------------------------------------------
void SetADSRNoteColors(int channelIndex, int noteIndex)
{
    int midiNote = (int)adsrCurrentNotes[channelIndex][noteIndex];
    int octave = midiNote/12;     //octaves are 0-indexed (0-9, 10 is controls)
    int scaleNote = midiNote%12;
    int colorHue;
    int colorSaturation; 
  //CH 1---------------------------------------------
  if(channelIndex == 0)   //midiChannel 1 ("Drums")
  {
    if(octave < 2)    //midiNote < 24
    {
      if(octave == 0){colorSaturation = 100;}
      else if(octave == 1){colorSaturation = 175;}
      colorHue = map(scaleNote, 0, 11, 0, 234);
    }
    else if(octave == 2)  //midiNote 24-35 (blue-red (sub kicks))
    {
      colorSaturation = 255;
      colorHue = map(scaleNote, 0, 11, 160, 248);   //blue to (red(minus increment)) ~7 step hues
    }
    else if(octave == 3)  //midiNote 36-47 (general MIDI Drums)
    {
      if(midiNote == 36){colorHue = 0; colorSaturation = 255;} //Kick(red)
      else if(midiNote == 37){colorHue = 224; colorSaturation = 150;} //SideStick(light pink)
      else if(midiNote == 38){colorHue = 0; colorSaturation = 0;}   //Snare(white)
      else if(midiNote == 39){colorHue = 64; colorSaturation = 255;}  //Alt Snare 1(yellow)
      else if(midiNote == 40){colorHue = 130; colorSaturation = 150;} //Alt Snare 2(desaturated aqua)
      else if(midiNote == 41){colorHue = 192; colorSaturation = 255;}  //Low Tom(purple)
      else if(midiNote == 42){colorHue = 64; colorSaturation = 150;}  //Hat (desaturated yellow)
      else if(midiNote == 43){colorHue = 160; colorSaturation = 255;}  //Mid-Low Tom(Blue)
      else if(midiNote == 44){colorHue = 32; colorSaturation = 150;}  //Alt Hat(desaturated orange)
      else if(midiNote == 45){colorHue = 128; colorSaturation = 255;}  //Mid-High Tom(Green/Blue)
      else if(midiNote == 46){colorHue = 32; colorSaturation = 255;}  //Open Hat(orange)
      else if(midiNote == 47){colorHue = 96; colorSaturation = 255;}  //High Tom(Green)
    }
    else if(octave > 3)
    {
      colorSaturation = 255;
      colorHue = map(midiNote, 48, 119, 0, 250);    //Big-ass rainbow spectrum -- (3.5 step hues)
    }
  }
  //CH 2---------------------------------------------
  else if(channelIndex == 1)    //midiChannel 2 ("Bass")
  {
    if(octave < 3)   //full saturation 3-octave rainbow
    {
      colorHue = map(midiNote, 1, 35, 0, 248);
      colorSaturation = 255;
    }
    else if(octave > 5)  //desaturating octave rainbows  
    {
      colorHue = map(scaleNote, 0, 11, 0, 234);   //single octave chromatic rainbow -- (~21 step hues)
      if(octave == 6){colorSaturation = 225;}
      else if(octave == 7){colorSaturation = 190;}
      else if(octave == 8){colorSaturation = 150;}
      else if(octave == 9){colorSaturation = 100;}
      //else if(octave == 10){colorSaturation = 30;} octave 10 is now controls
    }
    else if(octave > 2 && octave < 6)   //Scriabin(circle of 5ths), -8 below octave, +8 above octave
    {
      colorSaturation = 255;
      switch(scaleNote)   
      {
        case 0:   //C
          if(octave == 3){colorHue = 247;}
          else if(octave == 4){colorHue = 0;}   //Scriabin
          else if(octave == 5){colorHue = 8;}
          break;
        case 1:   //C#
          if(octave == 3){colorHue = 136;}
          else if(octave == 4){colorHue = 144;}   //Scriabin
          else if(octave == 5){colorHue = 152;}
          break;
        case 2:   //D
          if(octave == 3){colorHue = 56;}
          else if(octave == 4){colorHue = 64;}   //Scriabin
          else if(octave == 5){colorHue = 72;}
          break;
        case 3:   //Eb
          if(octave == 3){colorHue = 168;}
          else if(octave == 4){colorHue = 176;}   //Scriabin
          else if(octave == 5){colorHue = 184;}
          break;
        case 4:   //E
          if(octave == 3){colorHue = 88;}
          else if(octave == 4){colorHue = 96;}   //Scriabin
          else if(octave == 5){colorHue = 104;}
          break;
        case 5:   //F
          if(octave == 3){colorHue = 216;}
          else if(octave == 4){colorHue = 224;}   //Scriabin
          else if(octave == 5){colorHue = 232;}
          break;
        case 6:   //F#
          if(octave == 3){colorHue = 120;}
          else if(octave == 4){colorHue = 128;}   //Scriabin
          else if(octave == 5){colorHue = 136;}
          break;
        case 7:   //G
          if(octave == 3){colorHue = 24;}
          else if(octave == 4){colorHue = 32;}   //Scriabin
          else if(octave == 5){colorHue = 40;}
          break;
        case 8:   //Ab
          if(octave == 3){colorHue = 152;}
          else if(octave == 4){colorHue = 160;}   //Scriabin
          else if(octave == 5){colorHue = 168;}
          break;
        case 9:   //A
          if(octave == 3){colorHue = 72;}
          else if(octave == 4){colorHue = 80;}   //Scriabin
          else if(octave == 5){colorHue = 88;}
          break;
        case 10:  //Bb
          if(octave == 3){colorHue = 184;}
          else if(octave == 4){colorHue = 192;}   //Scriabin
          else if(octave == 5){colorHue = 200;}
          break;
        case 11:  //B 
          if(octave == 3){colorHue = 104;}
          else if(octave == 4){colorHue = 112;}   //Scriabin
          else if(octave == 5){colorHue = 120;}
          break;
      }
    }
  }
  //CH 3-4-------------------------------------------
  else if(channelIndex == 2 || channelIndex == 3) //midiChannel 3 ("Piano") and 4 ("Pads/SoftSynth") same coloring
  {
    if(midiNote > 35 && midiNote < 85)
    {
      colorHue = map(midiNote, 36, 84, 0, 224);         //4-octave chromatic C2-C6... pretty cool(C6 is desaturated)
    }
    else if(midiNote < 36)
    {
      colorHue = map(midiNote, 1, 35, 192, 253);        //reversing to purple hue, full saturation....ehhhh
    }
    else if(midiNote > 84)
    {
      colorHue = map(midiNote, 85, 119, 226, 160);      //reversing to blue, desaturating heavily....ok
    }
    
    
    switch(octave)
    {
      case 0:
          colorSaturation = 255;
          break;
      case 1:
          colorSaturation = 255;
          break;
      case 2:
          colorSaturation = 255;
          break;
      case 3:   //start of 4-octave rainbow
          colorSaturation = 255;
          break;
      case 4:
          colorSaturation = 250;
          break;
      case 5:
          colorSaturation = 245;
          break;
      case 6:   //last octave of rainbow
          colorSaturation = 245-(scaleNote*5);
          break;
      case 7:   //C6 included in rainbow, C# begins reversing hue
          colorSaturation = 185-(scaleNote*5);
          break;
      case 8:
          colorSaturation = 125-(scaleNote*5);
          break;
      case 9:
          colorSaturation = 65-(scaleNote*5);
          break;
    }
  }
  //CH 5-8-------------------------------------------
  else if(channelIndex > 3 && channelIndex < 8)   //index 4-7, midiChannel 5-8 (Full Spectrum with ADSR)
  {
      colorHue = map(midiNote,  1, 119, 0, 252);    //~2-hue step (0-252)
      colorSaturation = 255;
  }
  //---------------------------------------------
  //---------------------------------------------
    colorHue = constrain(colorHue, 0, 255);
    int adjustedSaturation = (float)colorSaturation*((float)channelSaturation[channelIndex]/255.0);
    adsrNoteHSV[channelIndex][noteIndex].h = (byte)colorHue;
    adsrNoteHSV[channelIndex][noteIndex].s = (byte)adjustedSaturation; 
    return;
}

//------------------------------------------------------------------------------------------------------------------
//-------------------------------------------ADSR Control Functions-------------------------------------------------
void ResetDefaultADSR(int channelIndex)
{
    atkFrames[channelIndex] = defaultAtkFrames[channelIndex];
    holdFrames[channelIndex] = defaultHoldFrames[channelIndex];
    decayFrames[channelIndex] = defaultDecayFrames[channelIndex];
    sustainValue[channelIndex] = defaultSustainValue[channelIndex];
    releaseFrames[channelIndex] = defaultReleaseFrames[channelIndex];
    return;
}

//------------------------------------------------------------------------------------------------------------------
void SetNewADSR(int channelIndex, int adsrType, byte midiVelocity) //adsrType(atk=0, decay=1, sustain=2, release =3)
{
    int newFrames = map(midiVelocity, 1, 127, 300, 1);
    float newSusValue = (float)midiVelocity/127.0;
    
    switch(adsrType)
    {
      case 0:
          atkFrames[channelIndex] = newFrames;
          break;
      case 1:
          decayFrames[channelIndex] = newFrames;
          break;
      case 2:
          sustainValue[channelIndex] = newSusValue;
          break;
      case 3:
          releaseFrames[channelIndex] = newFrames;
          break;
    }
    return;
}

//------------------------------------------------------------------------------------------------------------------
//-------------------------------------------Gradient Control Functions---------------------------------------------
void ResetDefaultBlendAmounts(int gradientChannelIndex)
{
    blendAmounts[gradientChannelIndex] = defaultBlendAmounts[gradientChannelIndex];
    return;
}
//------------------------------------------------------------------------------------------------------------------
//-------------------------------------------MIDI ADSR Storage------------------------------------------------------
void ActivateNote(byte midiChannel, byte midiNote, byte midiVelocity, int noteIndex)
{
    adsrCurrentNotes[midiChannel][noteIndex] = midiNote;
    adsrCurrentVelocities[midiChannel][noteIndex] = midiVelocity;
    adsrNoteActive[midiChannel][noteIndex] = true;              
    adsrCurrentFrame[midiChannel][noteIndex] = 0;
    atkOn[midiChannel][noteIndex] = true;
    holdOn[midiChannel][noteIndex] = false;
    decayOn[midiChannel][noteIndex] = false;
    susOn[midiChannel][noteIndex] = false;
    releaseOn[midiChannel][noteIndex] = false;
    
    SetADSRNoteColors((int)midiChannel, noteIndex);
    adsrNoteHSV[midiChannel][noteIndex].v = map(midiVelocity, 1, 127, 1, noteBrightConstant);
    
    return;
}

//------------------------------------------------------------------------------------------------------------------
//---------------------------------------------MIDI Callbacks-------------------------------------------------------
void HandleNoteOn(byte midiChannel, byte midiNote, byte midiVelocity)
{
//---------Handle Scope
  if(midiChannel>NUM_ADSR_CHANNELS)
  {
    GradientNoteOn(midiChannel, midiNote, midiVelocity);
    return;
  }
  midiChannel--;                      //MIDI library returns 1-16, I need 0-15 for array index
//---------Handle Controls
  if(midiNote == 0)   //channel cut
  {
    for(int i=0; i<MAX_ADSR_POLY; i++)
    {
      adsrNoteActive[midiChannel][i] = false;
      adsrNoteHSV[midiChannel][i].v = 0;
    }
    return;
  }
  else if(midiNote > 119)   //octave 10 controls 
  {
    if(midiNote == 120){SetNewADSR(midiChannel, 0, midiVelocity);}//set channel Attack value with velocity
    else if(midiNote == 121){SetNewADSR(midiChannel, 1, midiVelocity);}//set channel Decay value with velocity
    else if(midiNote == 122){SetNewADSR(midiChannel, 2, midiVelocity);}//set channel Sustain value with velocity
    else if(midiNote == 123){SetNewADSR(midiChannel, 3, midiVelocity);}//set channel Release value with velocity
    else if(midiNote == 124){ResetDefaultADSR(midiChannel);}//reset ADSR to default
    else if(midiNote == 125){}//channel control function 1(set noteBrightness Scale)
    else if(midiNote == 126){}  //channel control 2
    else if(midiNote == 127){}  //channel control 3
    return;
  }
//-------Activate Real Notes
  else
  {
      for(int i=0; i<=MAX_ADSR_POLY; i++)      //MAX_ADSR_POLY -1 is the max index
        {
          if(i==MAX_ADSR_POLY) //1 too many(index), no open notes found  !!!Important that this is first!!!
          {
            ActivateNote(midiChannel, midiNote, midiVelocity, adsrLastNoteWritten[midiChannel]);
            break;
          }
          else if(adsrNoteActive[midiChannel][i]==false)
          {
            ActivateNote(midiChannel, midiNote, midiVelocity, i);
            adsrLastNoteWritten[midiChannel] = i;
            break;
          }
        }
    return;
  }
}

//------------------------------------------------------------------------------------------------------------------
void HandleNoteOff(byte midiChannel, byte midiNote, byte midiVelocity)
{
//------Handle Scope
  if(midiChannel>NUM_ADSR_CHANNELS)   //pass to gradient handling
  {
    GradientNoteOff(midiChannel, midiNote, midiVelocity);
    return;
  }
  midiChannel--;                      //MIDI library returns 1-16, I need 0-15 for array index
//------Ignore Controls
  if(midiNote == 0 || midiNote > 119){return;}
//------Handle Real Note Off's
  else
  {
    for(int i=0; i<MAX_ADSR_POLY; i++)       //MAX_ADSR_POLY -1 is the max index, no need to go beyond for NoteOff
    {
      if(midiNote==adsrCurrentNotes[midiChannel][i] && releaseOn[midiChannel][i]==false)
      {
        adsrCurrentFrame[midiChannel][i] = 0;
        atkOn[midiChannel][i] = false;
        holdOn[midiChannel][i] = false;
        decayOn[midiChannel][i] = false;
        susOn[midiChannel][i] = false;
        releaseOn[midiChannel][i] = true;
        //adsrLastBrightness[midiChannel][i] = adsrNoteHSV[midiChannel][i].v;
        adsrBrightnessCVsAtRelease[midiChannel][i] = adsrBrightnessCVs[midiChannel][i];
      }
    }
    return;
  }
}

//------------------------------------------------------------------------------------------------------------------
void HandleControlChange(byte midiChannel, byte controlNumber, byte controlValue)
{
    midiChannel--;    //program returns 1-16, I need 0-15 index
    if(controlNumber == 1)  //modulation wheel --- desaturates to max
    {
      channelSaturation[midiChannel] = map(controlValue, 0, 127, 255, 0);
    }
    return;
}

//------------------------------------------------------------------------------------------------------------------
//-------------------------------------Gradient Note Handling-------------------------------------------------------
void GradientNoteOn(byte midiChannel, byte midiNote, byte midiVelocity)
{
//Handle Scope---------------------
    if(midiChannel > (NUM_ADSR_CHANNELS + NUM_GRADIENT_CHANNELS))
    {
      //pass to animation handling
      return;
    }
    midiChannel--;                                                //correct for general index
    int gradientChannelIndex = (midiChannel - NUM_ADSR_CHANNELS); //set gradient index
//Handle Controls------------------
    if(midiNote == 0)   //channel cut
    {
      fill_solid(channelRGBData[midiChannel], NUM_LEDS, CRGB::Black);
      gradientChannelActive[gradientChannelIndex] = false;
      for(int i=0; i<MAX_GRADIENT_POLY; i++)
      {
        gradientNoteActive[gradientChannelIndex][i] = false;
      }
    }
    else if(midiNote > 119)   //octave 10 controls
    {
      if(midiNote == 120){}     //channel control
      else if(midiNote == 121){}//channel control
      else if(midiNote == 122){}//channel control
      else if(midiNote == 123){}//channel control
      else if(midiNote == 124){}//channel control
      else if(midiNote == 125){}//channel control
      else if(midiNote == 126){}//channel control 
      else if(midiNote == 127){}//channel control 
      return;
    }
//Handle Real Notes
    else
    {
      for(int i=0; i<MAX_GRADIENT_POLY; i++)
      {
        if(gradientNoteActive[gradientChannelIndex][i] == false)
        {
          gradientCurrentNotes[gradientChannelIndex][i] = midiNote;
          gradientChannelActive[gradientChannelIndex] = true;
          gradientNoteActive[gradientChannelIndex][i] = true;
          gradientHasChanged[gradientChannelIndex] = true;
          int colorHue = map(midiNote, 1, 119, 0, 253);
          midiVelocity = constrain(midiVelocity, 65, 127);  //*PLACEHOLDER* until I can get the slow fades adjusted
          int colorBrightness = map(midiVelocity, 1, 127, 1, 255);
          gradientPointValues[gradientChannelIndex][i] = CHSV((byte)colorHue,
                                                              channelSaturation[midiChannel],
                                                              (byte)colorBrightness);
          break;
        }
      }
    }
    return;
}

//------------------------------------------------------------------------------------------------------------------
void GradientNoteOff(byte midiChannel, byte midiNote, byte midiVelocity)
{
//Handle Scope---------------------
    if(midiChannel > (NUM_ADSR_CHANNELS + NUM_GRADIENT_CHANNELS))
    {
      //pass to animation handling
      return;
    }
    midiChannel--;                                                //correct for general index
    int gradientChannelIndex = (midiChannel - NUM_ADSR_CHANNELS); //set gradient index
//Ignore Controls------------------
    if(midiNote == 0 || midiNote > 119){return;}
//Handle Real Notes
    else
    {
      for(int i=0; i<MAX_GRADIENT_POLY; i++)
      {
        if(gradientCurrentNotes[gradientChannelIndex][i] == midiNote)
        {
          gradientNoteActive[gradientChannelIndex][i] = false;
          gradientPointValues[gradientChannelIndex][i] = CHSV(0,0,0);
          gradientHasChanged[gradientChannelIndex] = true;
          break;
        }
      }
    }
    return;
}
//==================================================================================================================
void setup() 
{
    usbMIDI.begin();
    usbMIDI.setHandleNoteOn(HandleNoteOn);
    usbMIDI.setHandleNoteOff(HandleNoteOff);
    usbMIDI.setHandleControlChange(HandleControlChange);
    
    //LEDS.addLeds<WS2812SERIAL,DATA_PIN,RGB>(leds,NUM_LEDS);
    FastLED.addLeds<NUM_STRIPS, WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(60);
    FastLED.clear(true);      //push out a black strip

    //----Push default values to variable storage
    for(int i=0; i<NUM_ADSR_CHANNELS; i++)
    {
      ResetDefaultADSR(i);
    }
    for(int i=0; i<NUM_GRADIENT_CHANNELS; i++)
    {
      ResetDefaultBlendAmounts(i);
    }
}

//==================================================================================================================
//------------------------------------------------------------------------------------------------------------------
//==================================================================================================================
void loop() 
{
    usbMIDI.read();
    EVERY_N_MILLISECONDS(GRADIENT_FRAME_TIME)   //currently 33.333fps
    {
      ScanActiveGradientNotes();
      GenerateTargetChannelGradients();
      BlendTowardTargetGradients();   //fades between gradient changes and pushes to channelRGBs
    }
    EVERY_N_MILLISECONDS(ADSR_FRAME_TIME)       //currently 200fps
    {
      ScanActiveADSRNotes();    //handle framing, ADSR, push values to adsrNoteRGBData
      SumADSRNoteRGBtoChannels();   //variable summing structure to channel pixel arrays
      SumChannelRGBtoMain();    //push to main "leds" array, channels also accesible for breakout display
      FastLED.show();
    }
}
//==================================================================================================================
//--------------------------------------------LOOP FLOW FUNCTIONS---------------------------------------------------
void ScanActiveADSRNotes()
{
    for(int i=0; i<NUM_ADSR_CHANNELS; i++)     //NUM_ADSR_CHANNELS -1 is max index
      {
        for(int j=0; j<MAX_ADSR_POLY; j++)       //MAX_ADSR_POLY -1 is max index
        {
          if(adsrNoteActive[i][j])
          {
            HandleADSR(i,j);             //handles frames and calls AHDSR function
            //------
            adsrNoteRGBData[i][j] 
            = 
            CHSV(adsrNoteHSV[i][j].h, adsrNoteHSV[i][j].s, (adsrNoteHSV[i][j].v*adsrBrightnessCVs[i][j]));
            //------
          }
          else
          {
            adsrNoteRGBData[i][j] = CRGB::Black;
          }
        }
      }
      return; 
}

//------------------------------------------------------------------------------------------------------------------
void SumADSRNoteRGBtoChannels()
{
    for(int i=0; i<NUM_ADSR_CHANNELS; i++)
    {
      fill_solid(channelRGBData[i], NUM_LEDS, CRGB::Black);   //clear channel data
      for(int j=0; j<MAX_ADSR_POLY; j++)
      {
        channelRGBData[i][0] += adsrNoteRGBData[i][j];
      }
      fill_solid(channelRGBData[i], NUM_LEDS, channelRGBData[i][0]);
    }
    return;
}

//------------------------------------------------------------------------------------------------------------------
void ScanActiveGradientNotes()
{
    for(int i=0; i<NUM_GRADIENT_CHANNELS; i++)
    {
      numberActiveGradientPoints[i] = 0;
      int indexCounter = 0;
      for(int j=0; j<MAX_GRADIENT_POLY; j++)
      {
        if(gradientNoteActive[i][j])
        {
          activeGradientIndexes[i][indexCounter] = j;
          numberActiveGradientPoints[i]+=1;
          indexCounter++;
        }
      }
    }
}

//------------------------------------------------------------------------------------------------------------------
void GenerateTargetChannelGradients()
{
    for(int i=0; i<NUM_GRADIENT_CHANNELS; i++)
    {
      if(gradientChannelActive[i] && gradientHasChanged[i])
      {
        switch(numberActiveGradientPoints[i])
        {
          case 0:
              fill_solid(targetGradientRGB[i], NUM_LEDS, CRGB::Black);
              break;
          case 1:   //damn... desaturated/half bright-to full color in middle-to desaturated/half bright
              fill_gradient(targetGradientRGB[i], NUM_LEDS,
                            //--- 
                            CHSV(gradientPointValues[i][activeGradientIndexes[i][0]].h,
                                 50,
                                 (gradientPointValues[i][activeGradientIndexes[i][0]].v/2)),
                            //---
                            gradientPointValues[i][activeGradientIndexes[i][0]],
                            //---
                            CHSV(gradientPointValues[i][activeGradientIndexes[i][0]].h,
                                 50,
                                 (gradientPointValues[i][activeGradientIndexes[i][0]].v/2)));
                            //---
              break;
          case 2:   //color one to color 2
              fill_gradient(targetGradientRGB[i], NUM_LEDS, 
                            gradientPointValues[i][activeGradientIndexes[i][0]],
                            gradientPointValues[i][activeGradientIndexes[i][1]]);
              break;
          case 3:
              fill_gradient(targetGradientRGB[i], NUM_LEDS, 
                            gradientPointValues[i][activeGradientIndexes[i][0]],
                            gradientPointValues[i][activeGradientIndexes[i][1]],
                            gradientPointValues[i][activeGradientIndexes[i][2]]);
              break;
          case 4:
              fill_gradient(targetGradientRGB[i], NUM_LEDS, 
                            gradientPointValues[i][activeGradientIndexes[i][0]],
                            gradientPointValues[i][activeGradientIndexes[i][1]],
                            gradientPointValues[i][activeGradientIndexes[i][2]],
                            gradientPointValues[i][activeGradientIndexes[i][3]]);
              break;
        }
        gradientHasChanged[i] = false;
      }
    }
}

//------------------------------------------------------------------------------------------------------------------
void BlendTowardTargetGradients()
{
    for(int i=0; i<NUM_GRADIENT_CHANNELS; i++)
    {
      if(gradientChannelActive[i])
      {
        for(int j=0; j<NUM_LEDS; j++)
        {
          if(currentGradientRGB[i][j] != targetGradientRGB[i][j])
          {
            nblend(currentGradientRGB[i][j], targetGradientRGB[i][j], blendAmounts[i]);
            channelRGBData[i+NUM_ADSR_CHANNELS][j] = currentGradientRGB[i][j];
          }
        }
      }
    }
}

//------------------------------------------------------------------------------------------------------------------
void SumChannelRGBtoMain()
{
    FastLED.clear();    //clear main array pixels
    for(int i=0; i<NUM_CHANNELS; i++)
    {
      for(int j=0; j<NUM_LEDS; j++)
      {
        leds[j] += channelRGBData[i][j];
      }
    }
    return;
}
