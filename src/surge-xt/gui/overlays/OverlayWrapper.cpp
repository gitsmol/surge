/*
 ** Surge Synthesizer is Free and Open Source Software
 **
 ** Surge is made available under the Gnu General Public License, v3.0
 ** https://www.gnu.org/licenses/gpl-3.0.en.html
 **
 ** Copyright 2004-2021 by various individuals as described by the Git transaction log
 **
 ** All source at: https://github.com/surge-synthesizer/surge.git
 **
 ** Surge was a commercial product from 2004-2018, with Copyright and ownership
 ** in that period held by Claes Johanson at Vember Audio. Claes made Surge
 ** open source in September 2018.
 */

#include "OverlayWrapper.h"
#include "RuntimeFont.h"
#include "SurgeImage.h"

namespace Surge
{
namespace Overlays
{
OverlayWrapper::OverlayWrapper()
{
    closeButton = std::make_unique<juce::TextButton>("closeButton");
    closeButton->addListener(this);
    closeButton->setButtonText("X");
    addAndMakeVisible(*closeButton);
}

OverlayWrapper::OverlayWrapper(const juce::Rectangle<int> &cb) : OverlayWrapper()
{
    componentBounds = cb;
    isModal = true;
}

void OverlayWrapper::paint(juce::Graphics &g)
{
    auto sp = getLocalBounds();
    if (isModal)
    {
        sp = componentBounds;
        g.fillAll(skin->getColor(Colors::Overlay::Background));
    }

    g.setColour(skin->getColor(Colors::Dialog::Titlebar::Background));
    g.fillRect(sp);
    g.setColour(skin->getColor(Colors::Dialog::Titlebar::Text));
    g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(10, juce::Font::bold));
    g.drawText(title, sp.withHeight(titlebarSize + margin), juce::Justification::centred);

    if (icon)
    {
        icon->drawAt(g, sp.getX() + 2, sp.getY() + 1, 1);
    }

    g.setColour(skin->getColor(Colors::Dialog::Border));
    g.drawRect(sp, 1);
}

void OverlayWrapper::addAndTakeOwnership(std::unique_ptr<juce::Component> c)
{
    auto sp = getLocalBounds();
    if (isModal)
        sp = componentBounds;

    auto q = sp.reduced(2 * margin, 2 * margin)
                 .withTrimmedBottom(titlebarSize)
                 .translated(0, titlebarSize + 0 * margin);
    primaryChild = std::move(c);
    primaryChild->setBounds(q);

    auto buttonSize = titlebarSize - 2;
    auto closeButtonBounds =
        getLocalBounds().withHeight(buttonSize).withLeft(getWidth() - buttonSize).translated(-2, 2);

    if (showCloseButton)
    {
        closeButton->setBounds(closeButtonBounds);
    }
    else
    {
        closeButton->setVisible(false);
    }

    addAndMakeVisible(*primaryChild);
}

void OverlayWrapper::buttonClicked(juce::Button *button)
{
    if (button == closeButton.get())
    {
        closeOverlay();
    }
}

} // namespace Overlays
} // namespace Surge
