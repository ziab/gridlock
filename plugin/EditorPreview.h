#include "DrillPanel.h"
#pragma once

#include "PluginEditor.h"

// Render the real editor without a native window or audio device.
// Usage: LogicTests.exe --render-ui <absolute-output-directory>
struct EditorPreview {
  static void verifyControls (juce::UnitTest &test) {
    MidiGridAnalyzerAudioProcessor processor;
    MidiGridAnalyzerAudioProcessorEditor editor (processor);
    editor.openGLContext.detach ();
    editor.stopTimer ();
    auto &state = processor.getAPVTS ();
    test.beginTest ("Segmented bars preserve parameter binding in both directions");
    editor.barButtons[0].onClick ();
    test.expectEquals ((int)state.getRawParameterValue ("bars_window")->load (), 0);
    state.getParameter ("bars_window")->setValueNotifyingHost (1.0f);
    juce::MessageManager::getInstance ()->runDispatchLoopUntil (20);
    test.expect (editor.barButtons[3].getToggleState ());
    test.expect (!editor.barButtons[0].getToggleState ());

    test.beginTest ("Hidden label controls still update parameters");
    editor.showVelButton.setToggleState (true, juce::sendNotificationSync);
    test.expectEquals (state.getRawParameterValue ("show_velocity_labels")->load (), 1.0f);

    test.beginTest ("Settings preserve grid space and scroll to the final control");
    editor.setSize (900, 480);
    const int closedWidth = editor.gridComponent.getWidth ();
    editor.settingsButton.setToggleState (true, juce::dontSendNotification);
    editor.settingsButton.onClick ();
    test.expect (editor.settingsViewport.isVisible ());
    test.expect (editor.gridComponent.getWidth () < closedWidth);
    test.expect (!editor.labelsButton.getBounds ().intersects (editor.copyTabButton.getBounds ()));
    editor.settingsViewport.setViewPosition (0, editor.settingsContent.getHeight ());
    test.expect (editor.settingsViewport.getViewArea ().contains (editor.testButton.getBounds ()));
    editor.settingsButton.setToggleState (false, juce::dontSendNotification);
    editor.settingsButton.onClick ();
    test.expectEquals (editor.gridComponent.getWidth (), closedWidth);
    test.expect (!editor.settingsViewport.isVisible ());

    test.beginTest ("Readable mix values retain their underlying parameter units");
    test.expectEquals (editor.clickVolumeSlider.getTextFromValue (0.8), juce::String ("80%"));
    test.expectWithinAbsoluteError (editor.clickVolumeSlider.getValueFromText ("125%"), 1.25, 0.001);
    test.expectEquals (editor.clickPanSlider.getTextFromValue (0.0), juce::String ("Centre"));
    test.expectWithinAbsoluteError (editor.clickPanSlider.getValueFromText ("25 L"), -0.25, 0.001);
  }

  static bool render (const juce::File &directory) {
    if (directory.createDirectory ().failed ()) {
      return false;
    }
    MidiGridAnalyzerAudioProcessor processor;
    processor.prepareToPlay (constants::params::sampleRateFallback, 512);
    MidiGridAnalyzerAudioProcessorEditor editor (processor);
    editor.openGLContext.detach ();
    editor.stopTimer ();
    processor.getAPVTS ().getParameter ("test_mode")->setValueNotifyingHost (1.0f);
    juce::AudioBuffer<float> audio (2, 512);
    for (int block = 0; block < 640; ++block) {
      juce::MidiBuffer midi;
      processor.processBlock (audio, midi);
      editor.timerCallback ();
    }
    // First paint starts JUCE's startup badge timer; capture the settled UI after its normal fade.
    editor.createComponentSnapshot (editor.getLocalBounds ());
    juce::MessageManager::getInstance ()->runDispatchLoopUntil (4500);
    struct View {
      const char *name;
      int width;
      int height;
      bool settings;
      int scroll;
    };
    const View views[] = {{"desktop", 1640, 900, false, 0},
                          {"settings", 1640, 1000, true, 0},
                          {"compact", 900, 480, false, 0},
                          {"compact-settings", 900, 480, true, 0},
                          {"settings-scrolled", 900, 480, true, 730}};
    for (const auto &view : views) {
      editor.settingsButton.setToggleState (view.settings, juce::dontSendNotification);
      editor.setSize (view.width, view.height);
      editor.resized ();
      editor.settingsViewport.setViewPosition (0, view.scroll);
      auto snapshot = editor.createComponentSnapshot (editor.getLocalBounds ());
      auto stream = directory.getChildFile (juce::String (view.name) + ".png").createOutputStream ();
      if (stream == nullptr || !stream->setPosition (0) ||
          !juce::PNGImageFormat ().writeImageToStream (snapshot, *stream)) {
        return false;
      }
      stream->truncate ();
    }
    const bool drillRendered = renderDrill (processor, directory);
    processor.releaseResources ();
    return drillRendered;
  }
  static bool renderDrill (MidiGridAnalyzerAudioProcessor &processor, const juce::File &directory) {
    processor.remoteServer.reset ();
    DrillPanel panel (processor);
    panel.createComponentSnapshot (panel.getLocalBounds ());
    juce::MessageManager::getInstance ()->runDispatchLoopUntil (120);
    for (int state = 0; state < 2; ++state) {
      if (state == 1) {
        auto &v = processor.drill.view;
        v.config.length = 6;
        juce::String ("RLRLKK").copyToUTF8 (v.config.pattern.data (), v.config.pattern.size ());
        v.state = DrillEngine::State::Playing;
        v.bpm = 84;
        v.best = 81;
        v.activeSlot = 3;
        v.passes = 1;
        v.progress = 0.65;
        v.accuracy = 0.96;
        v.nextBpm = 87;
        processor.publishDrillSnapshot ();
        juce::MessageManager::getInstance ()->runDispatchLoopUntil (120);
      }
      auto image = panel.createComponentSnapshot (panel.getLocalBounds ());
      auto stream = directory.getChildFile (state == 0 ? "drill-setup.png" : "drill-live.png").createOutputStream ();
      if (!stream || !stream->setPosition (0) || !juce::PNGImageFormat ().writeImageToStream (image, *stream)) {
        return false;
      }
      stream->truncate ();
    }
    return true;
  }
};
