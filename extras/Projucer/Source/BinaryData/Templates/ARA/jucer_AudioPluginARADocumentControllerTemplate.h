/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for an ARA document controller implementation. 

  ==============================================================================
*/

#pragma once

%%app_headers%%

//==============================================================================
/**
*/
class %%aradocumentcontroller_class_name%%  : public ARADocumentController
{
public:
    %%aradocumentcontroller_class_name%%(const ARA::ARADocumentControllerHostInstance* instance);
    ~%%aradocumentcontroller_class_name%%();

//==============================================================================
// Override document controller methods here
protected:


private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (%%aradocumentcontroller_class_name%%)
};
