#pragma once

#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

// Shared desktop control styling. Parameter attachments remain on JUCE controls.
class EditorLookAndFeel : public juce::LookAndFeel_V4 {
public:
  EditorLookAndFeel () {
    setDefaultSansSerifTypefaceName ("Segoe UI");
    setColour (juce::ComboBox::backgroundColourId, Theme::col (Theme::bgInput));
    setColour (juce::ComboBox::textColourId, Theme::col (Theme::textPrimary));
    setColour (juce::ComboBox::outlineColourId, Theme::col (Theme::border));
    setColour (juce::PopupMenu::backgroundColourId, Theme::col (Theme::bgHeader));
    setColour (juce::PopupMenu::textColourId, Theme::col (Theme::textPrimary));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::col (Theme::emerald).withAlpha (0.12f));
    setColour (juce::PopupMenu::highlightedTextColourId, Theme::col (Theme::emerald));
    setColour (juce::Slider::textBoxTextColourId, Theme::col (Theme::textPrimary));
    setColour (juce::Slider::textBoxBackgroundColourId, Theme::col (Theme::bgInput));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::highlightColourId, Theme::col (Theme::emerald).withAlpha (0.25f));
    setColour (juce::TextEditor::focusedOutlineColourId, Theme::col (Theme::emerald));
    setColour (juce::ScrollBar::thumbColourId, Theme::col (Theme::borderTrack));
  }

  juce::Font getComboBoxFont (juce::ComboBox &) override {
    return juce::Font (14.0f);
  }
  juce::Font getTextButtonFont (juce::TextButton &, int) override {
    return juce::Font (14.0f, juce::Font::bold);
  }
  juce::Font getPopupMenuFont () override {
    return juce::Font (14.0f);
  }

  void drawLabel (juce::Graphics &g, juce::Label &label) override {
    if (dynamic_cast<juce::Slider *> (label.getParentComponent ()) == nullptr) {
      juce::LookAndFeel_V4::drawLabel (g, label);
      return;
    }
    const auto bounds = label.getLocalBounds ().toFloat ().reduced (1.0f);
    g.setColour (Theme::col (Theme::bgInput));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (Theme::col (label.isBeingEdited () ? Theme::emerald : Theme::border));
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
    if (!label.isBeingEdited ()) {
      g.setFont (label.getFont ());
      g.setColour (Theme::col (Theme::textPrimary));
      g.drawFittedText (label.getText (), label.getLocalBounds ().reduced (4, 0), juce::Justification::centred, 1);
    }
  }

  void drawButtonBackground (juce::Graphics &g, juce::Button &button, const juce::Colour &, bool hover,
                             bool down) override {
    const auto bounds = button.getLocalBounds ().toFloat ().reduced (1.0f);
    const auto accent = button.findColour (juce::TextButton::buttonOnColourId);
    const bool active = button.getToggleState ();
    auto fill = active ? accent.withAlpha (0.13f) : Theme::col (Theme::bgInput);
    if (hover || down) {
      fill = fill.brighter (down ? 0.12f : 0.06f);
    }
    g.setColour (fill.withMultipliedAlpha (button.isEnabled () ? 1.0f : 0.4f));
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (button.hasKeyboardFocus (true) ? Theme::col (Theme::emerald)
                 : active                       ? accent.withAlpha (0.5f)
                                                : Theme::col (Theme::border));
    g.drawRoundedRectangle (bounds, 7.0f, 1.0f);
  }

  void drawComboBox (juce::Graphics &g, int width, int height, bool, int, int, int, int, juce::ComboBox &box) override {
    const auto bounds = juce::Rectangle<float> (0, 0, (float)width, (float)height).reduced (1.0f);
    g.setColour (Theme::col (Theme::bgInput));
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (Theme::col (box.hasKeyboardFocus (true) ? Theme::emerald : Theme::border));
    g.drawRoundedRectangle (bounds, 7.0f, 1.0f);
    juce::Path arrow;
    const float x = (float)width - 18.0f;
    const float y = (float)height / 2.0f;
    arrow.startNewSubPath (x - 3.0f, y - 2.0f);
    arrow.lineTo (x, y + 1.0f);
    arrow.lineTo (x + 3.0f, y - 2.0f);
    g.setColour (Theme::col (Theme::textMuted));
    g.strokePath (arrow, juce::PathStrokeType (1.5f));
  }

  void positionComboBoxText (juce::ComboBox &box, juce::Label &label) override {
    label.setBounds (10, 0, box.getWidth () - 34, box.getHeight ());
    label.setFont (getComboBoxFont (box));
  }

  juce::Label *createSliderTextBox (juce::Slider &slider) override {
    auto *label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::Font (slider.getName () == "Tempo" ? 30.0f : 14.0f, juce::Font::bold));
    return label;
  }

  void drawLinearSlider (juce::Graphics &g, int x, int y, int width, int height, float position, float, float,
                         juce::Slider::SliderStyle, juce::Slider &slider) override {
    const float centre = (float)y + (float)height / 2.0f;
    const float left = (float)x;
    const auto accent = Theme::col (Theme::emerald).withMultipliedAlpha (slider.isEnabled () ? 1.0f : 0.3f);
    g.setColour (Theme::col (Theme::borderTrack));
    g.fillRoundedRectangle (left, centre - 2.0f, (float)width, 4.0f, 2.0f);
    g.setColour (accent.withAlpha (0.65f));
    g.fillRoundedRectangle (left, centre - 2.0f, juce::jmax (0.0f, position - left), 4.0f, 2.0f);
    g.setColour (accent);
    g.fillEllipse (position - 5.0f, centre - 5.0f, 10.0f, 10.0f);
    if (slider.hasKeyboardFocus (true) || slider.isMouseOverOrDragging ()) {
      g.setColour (accent.withAlpha (0.2f));
      g.drawEllipse (position - 8.0f, centre - 8.0f, 16.0f, 16.0f, 2.0f);
    }
  }
};
