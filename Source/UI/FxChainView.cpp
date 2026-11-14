#include "FxChainView.h"

using namespace Params;

namespace
{
    constexpr float nodeHeight      = 58.0f;
    constexpr float minNodeWidth    = 72.0f;
    constexpr float maxNodeWidth    = 136.0f;
    constexpr float endpointWidth   = 100.0f;
    constexpr float addSize         = 34.0f;
    constexpr float normalGap       = 28.0f;
    constexpr int   dragThreshold   = 5;
}

//==============================================================================
FxChainView::FxChainView (SynthAudioProcessor& p)
    : UI::Section ("SIGNAL CHAIN", Palette::accentAlt),
      processor (p)
{
    auto& apvts = processor.getAPVTS();

    for (int i = 0; i < numEffects; ++i)
    {
        onParams[(size_t) i] = apvts.getParameter (effectInfo[(size_t) i].onParamID);
        jassert (onParams[(size_t) i] != nullptr);
        lastOnState[(size_t) i] = isEffectOn ((Effect) i);
    }

    chain = processor.getFxChain();

    if (! chain.empty())
        selected = chain.front();

    updateSubtitle();
    startTimerHz (15);
}

FxChainView::~FxChainView()
{
    stopTimer();
}

//==============================================================================
void FxChainView::timerCallback()
{
    // Follow changes made elsewhere (e.g. a session or preset being loaded).
    if (! dragging)
    {
        auto latest = processor.getFxChain();

        if (latest != chain)
        {
            chain = std::move (latest);

            const bool selectionGone = selected.has_value()
                                    && std::find (chain.begin(), chain.end(), *selected) == chain.end();

            if (selectionGone || ! selected.has_value())
                select (chain.empty() ? std::nullopt : std::optional<Effect> (chain.front()));

            updateSubtitle();
            updateLayout();
            repaint();
        }
    }

    // Repaint when an effect is switched on/off (also from host automation).
    bool anyChanged = false;

    for (int i = 0; i < numEffects; ++i)
    {
        const bool on = isEffectOn ((Effect) i);

        if (on != lastOnState[(size_t) i])
        {
            lastOnState[(size_t) i] = on;
            anyChanged = true;
        }
    }

    if (anyChanged)
        repaint();
}

void FxChainView::updateSubtitle()
{
    setSubtitle (chain.empty() ? "click + to add your first effect"
                               : "sound flows left to right  -  drag to reorder, click to edit");
}

//==============================================================================
void FxChainView::resized()
{
    updateLayout();
}

void FxChainView::updateLayout()
{
    const auto area = getContentBounds().toFloat();
    const auto& order = displayOrder();
    const int n = (int) order.size();
    const bool canAdd = n < numEffects;
    const float h = juce::jmin (nodeHeight, area.getHeight());
    const float top = area.getCentreY() - h * 0.5f;

    auto fixedWidthFor = [n, canAdd] (float gap, float endpoint)
    {
        return 2.0f * endpoint + gap * (float) (n + 1) + (canAdd ? addSize + gap : 0.0f);
    };

    // Tighten the spacing when the chain gets long so every node still fits.
    layoutGap = normalGap;
    float endpoint = endpointWidth;

    if (n > 0 && (area.getWidth() - fixedWidthFor (layoutGap, endpoint)) / (float) n < minNodeWidth)
    {
        layoutGap = 16.0f;
        endpoint = 84.0f;
    }

    const float fixedWidth = fixedWidthFor (layoutGap, endpoint);
    nodeWidth = n > 0 ? juce::jlimit (56.0f, maxNodeWidth, (area.getWidth() - fixedWidth) / (float) n) : maxNodeWidth;

    const float totalWidth = fixedWidth + nodeWidth * (float) n;
    float x = juce::jmax (area.getX(), area.getCentreX() - totalWidth * 0.5f);

    inBox = { x, top, endpoint, h };
    x += endpoint + layoutGap;
    firstNodeX = x;

    nodeBoxes.clear();

    for (int i = 0; i < n; ++i)
    {
        nodeBoxes.push_back ({ x, top, nodeWidth, h });
        x += nodeWidth + layoutGap;
    }

    if (canAdd)
    {
        addBox = { x, area.getCentreY() - addSize * 0.5f, addSize, addSize };
        x += addSize + layoutGap;
    }
    else
    {
        addBox = {};
    }

    outBox = { x, top, endpoint, h };
}

//==============================================================================
juce::Rectangle<float> FxChainView::powerArea (juce::Rectangle<float> node)
{
    return juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ node.getX() + 15.0f, node.getY() + 15.0f });
}

juce::Rectangle<float> FxChainView::removeArea (juce::Rectangle<float> node)
{
    return juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ node.getRight() - 15.0f, node.getY() + 15.0f });
}

FxChainView::Hit FxChainView::findHit (juce::Point<float> pos) const
{
    if (! dragging)
    {
        for (int i = 0; i < (int) nodeBoxes.size(); ++i)
        {
            const auto box = nodeBoxes[(size_t) i];

            if (! box.contains (pos))
                continue;

            if (powerArea (box).expanded (5.0f).contains (pos))   return { Part::power, i };
            if (removeArea (box).expanded (5.0f).contains (pos))  return { Part::remove, i };

            return { Part::node, i };
        }
    }

    if (! addBox.isEmpty() && addBox.expanded (4.0f).contains (pos))  return { Part::add, -1 };
    if (inBox.contains (pos))                                         return { Part::audioIn, -1 };
    if (outBox.contains (pos))                                        return { Part::audioOut, -1 };

    return {};
}

std::optional<FxChainView::Effect> FxChainView::hoveredEffect() const
{
    const bool onNode = hover.part == Part::node || hover.part == Part::power || hover.part == Part::remove;

    if (onNode && juce::isPositiveAndBelow (hover.index, (int) chain.size()))
        return chain[(size_t) hover.index];

    return std::nullopt;
}

//==============================================================================
bool FxChainView::isEffectOn (Effect effect) const
{
    return onParams[(size_t) effect]->getValue() >= 0.5f;
}

void FxChainView::toggleEffect (Effect effect)
{
    auto* param = onParams[(size_t) effect];
    param->beginChangeGesture();
    param->setValueNotifyingHost (isEffectOn (effect) ? 0.0f : 1.0f);
    param->endChangeGesture();
    repaint();
}

void FxChainView::select (std::optional<Effect> effect)
{
    if (selected == effect)
        return;

    selected = effect;
    repaint();

    if (onSelectionChanged != nullptr)
        onSelectionChanged (selected);
}

void FxChainView::commit (std::vector<Effect> newChain)
{
    chain = std::move (newChain);
    processor.setFxChain (chain);

    updateSubtitle();
    updateLayout();
    repaint();
}

void FxChainView::addEffect (Effect effect)
{
    if (std::find (chain.begin(), chain.end(), effect) != chain.end())
        return;

    // New nodes start switched on, so they're heard straight away.
    if (! isEffectOn (effect))
        toggleEffect (effect);

    auto newChain = chain;
    newChain.push_back (effect);
    commit (std::move (newChain));
    select (effect);
}

void FxChainView::removeEffect (Effect effect)
{
    const auto it = std::find (chain.begin(), chain.end(), effect);

    if (it == chain.end())
        return;

    const auto index = (size_t) std::distance (chain.begin(), it);

    auto newChain = chain;
    newChain.erase (newChain.begin() + (std::ptrdiff_t) index);
    commit (std::move (newChain));

    // Select the neighbour that moved into its place (or nothing if the chain is empty).
    if (selected == effect)
        select (chain.empty() ? std::nullopt : std::optional<Effect> (chain[juce::jmin (index, chain.size() - 1)]));

    hover = {};
}

void FxChainView::showAddMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Add an effect");

    for (int i = 0; i < numEffects; ++i)
    {
        if (std::find (chain.begin(), chain.end(), (Effect) i) != chain.end())
            continue;

        juce::PopupMenu::Item item (effectInfo[(size_t) i].name);
        item.itemID = i + 1;
        item.shortcutKeyDescription = effectInfo[(size_t) i].description;   // shown dimmed on the right
        menu.addItem (std::move (item));
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                                  .withTargetScreenArea (localAreaToGlobal (addBox.toNearestInt())),
                        [safeThis = juce::Component::SafePointer<FxChainView> (this)] (int result)
                        {
                            if (safeThis != nullptr && result > 0)
                                safeThis->addEffect ((Effect) (result - 1));
                        });
}

//==============================================================================
juce::String FxChainView::getTooltip()
{
    switch (hover.part)
    {
        case Part::audioIn:  return "Audio in: the sound of your oscillators enters the chain here";
        case Part::audioOut: return "Audio out: what you hear";
        case Part::add:      return "Add an effect to the end of the chain";
        case Part::power:    return "Switch this effect on or off";
        case Part::remove:   return "Remove this effect from the chain";

        case Part::node:
            if (const auto effect = hoveredEffect())
            {
                const auto& info = effectInfo[(size_t) *effect];
                return juce::String (info.name) + ": " + info.description + ". Drag to move it, click to edit it.";
            }
            break;

        case Part::none:
            break;
    }

    return {};
}

void FxChainView::mouseMove (const juce::MouseEvent& e)
{
    const auto hit = findHit (e.position);

    if (hit == hover)
        return;

    hover = hit;

    switch (hit.part)
    {
        case Part::node:                            setMouseCursor (juce::MouseCursor::DraggingHandCursor); break;
        case Part::power: case Part::remove:
        case Part::add:                             setMouseCursor (juce::MouseCursor::PointingHandCursor); break;
        case Part::audioIn: case Part::audioOut:
        case Part::none:                            setMouseCursor (juce::MouseCursor::NormalCursor); break;
    }

    repaint();
}

void FxChainView::mouseExit (const juce::MouseEvent&)
{
    hover = {};
    repaint();
}

void FxChainView::mouseDown (const juce::MouseEvent& e)
{
    pressedIndex = -1;
    const auto hit = findHit (e.position);

    switch (hit.part)
    {
        case Part::node:
            pressedIndex = hit.index;
            grabOffset = e.position.x - nodeBoxes[(size_t) hit.index].getX();
            select (chain[(size_t) hit.index]);
            break;

        case Part::power:   toggleEffect (chain[(size_t) hit.index]); break;
        case Part::remove:  removeEffect (chain[(size_t) hit.index]); break;
        case Part::add:     showAddMenu(); break;

        case Part::audioIn:
        case Part::audioOut:
        case Part::none:
            break;
    }
}

void FxChainView::mouseDrag (const juce::MouseEvent& e)
{
    if (pressedIndex < 0 || chain.size() < 2)
        return;

    if (! dragging)
    {
        if (e.getDistanceFromDragStart() < dragThreshold)
            return;

        dragging = true;
        dragPreview = chain;
    }

    const auto dragged = chain[(size_t) pressedIndex];
    const int n = (int) chain.size();

    floatingX = juce::jlimit (inBox.getRight(), juce::jmax (inBox.getRight(), outBox.getX() - nodeWidth),
                              e.position.x - grabOffset);

    // The slot under the centre of the floating node is where it will land.
    const float centre = floatingX + nodeWidth * 0.5f;
    const int target = juce::jlimit (0, n - 1, (int) std::floor ((centre - firstNodeX + layoutGap * 0.5f)
                                                                 / (nodeWidth + layoutGap)));

    std::vector<Effect> preview;
    for (auto fx : chain)
        if (fx != dragged)
            preview.push_back (fx);

    preview.insert (preview.begin() + target, dragged);

    if (preview != dragPreview)
    {
        dragPreview = std::move (preview);
        updateLayout();
    }

    repaint();
}

void FxChainView::mouseUp (const juce::MouseEvent& e)
{
    if (dragging)
    {
        dragging = false;
        commit (dragPreview);
        mouseMove (e);
    }

    pressedIndex = -1;
}

//==============================================================================
void FxChainView::paint (juce::Graphics& g)
{
    UI::Section::paint (g);

    const auto& order = displayOrder();
    const float cy = inBox.getCentreY();

    // Arrows between consecutive elements show the direction the sound travels.
    std::vector<juce::Rectangle<float>> sequence { inBox };
    sequence.insert (sequence.end(), nodeBoxes.begin(), nodeBoxes.end());
    if (! addBox.isEmpty())
        sequence.push_back (addBox);
    sequence.push_back (outBox);

    g.setColour (Palette::textFaint);

    for (size_t i = 0; i + 1 < sequence.size(); ++i)
    {
        const float x0 = sequence[i].getRight() + 3.0f;
        const float x1 = sequence[i + 1].getX() - 3.0f;

        if (x1 - x0 > 8.0f)
            g.drawArrow ({ x0, cy, x1, cy }, 1.5f, 9.0f, 8.0f);
    }

    drawEndpoint (g, inBox, "AUDIO IN");
    drawEndpoint (g, outBox, "AUDIO OUT");

    const auto* draggedEffect = dragging ? &chain[(size_t) pressedIndex] : nullptr;

    for (size_t i = 0; i < order.size() && i < nodeBoxes.size(); ++i)
    {
        if (draggedEffect != nullptr && order[i] == *draggedEffect)
        {
            // Empty slot where the dragged node will land
            g.setColour (Palette::textFaint.withAlpha (0.6f));
            g.drawRoundedRectangle (nodeBoxes[i].reduced (1.0f), 8.0f, 1.5f);
            continue;
        }

        drawNode (g, nodeBoxes[i], order[i], false);
    }

    if (! addBox.isEmpty())
        drawAddButton (g);

    if (draggedEffect != nullptr)
    {
        const auto box = juce::Rectangle<float> (floatingX, inBox.getY(), nodeWidth, inBox.getHeight());
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 14, { 0, 4 }).drawForRectangle (g, box.toNearestInt());
        drawNode (g, box, *draggedEffect, true);
    }
}

void FxChainView::drawEndpoint (juce::Graphics& g, juce::Rectangle<float> box, const juce::String& text) const
{
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (box, 8.0f);
    g.setColour (Palette::panelOutline);
    g.drawRoundedRectangle (box.reduced (0.75f), 8.0f, 1.5f);

    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold).withKerningFactor (0.06f));
    g.drawFittedText (text, box.reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.75f);
}

void FxChainView::drawNode (juce::Graphics& g, juce::Rectangle<float> box, Effect effect, bool floating) const
{
    const bool isSelected = selected == effect;
    const bool on = isEffectOn (effect);
    const auto hovered = hoveredEffect();
    const bool isHovered = hovered.has_value() && *hovered == effect;

    auto fill = Palette::knobBody;
    if (isSelected || floating)  fill = fill.brighter (0.12f);
    else if (isHovered)          fill = fill.brighter (0.06f);

    g.setColour (fill);
    g.fillRoundedRectangle (box, 8.0f);

    if (isSelected)
    {
        g.setColour (Palette::accentAlt);
        g.drawRoundedRectangle (box.reduced (1.0f), 8.0f, 2.0f);
    }

    // Name (dimmed when the effect is switched off)
    g.setColour (on ? Palette::text : Palette::textFaint);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawFittedText (effectInfo[(size_t) effect].name,
                      box.reduced (8.0f, 0.0f).withTrimmedTop (16.0f).withTrimmedBottom (4.0f).toNearestInt(),
                      juce::Justification::centred, 1, 0.7f);

    // On/off dot: filled = on, ring = off
    const auto power = powerArea (box);

    if (on)
    {
        g.setColour (Palette::accentAlt);
        g.fillEllipse (power);
    }
    else
    {
        g.setColour (Palette::textFaint);
        g.drawEllipse (power.reduced (0.75f), 1.5f);
    }

    // Remove cross
    const auto cross = removeArea (box).reduced (2.0f);
    const bool crossHot = isHovered && hover.part == Part::remove;
    g.setColour (crossHot ? Palette::text : Palette::textFaint);
    g.drawLine ({ cross.getTopLeft(), cross.getBottomRight() }, 1.8f);
    g.drawLine ({ cross.getBottomLeft(), cross.getTopRight() }, 1.8f);
}

void FxChainView::drawAddButton (juce::Graphics& g) const
{
    const bool hot = hover.part == Part::add;

    g.setColour (hot ? Palette::accentAlt : Palette::knobBody);
    g.fillEllipse (addBox);

    if (chain.empty() && ! hot)
    {
        // Draw attention to the starting point
        g.setColour (Palette::accentAlt);
        g.drawEllipse (addBox.reduced (1.0f), 2.0f);
    }

    const auto c = addBox.getCentre();
    const float s = addBox.getWidth() * 0.22f;
    g.setColour (hot ? Palette::background : Palette::text);
    g.drawLine (c.x - s, c.y, c.x + s, c.y, 2.2f);
    g.drawLine (c.x, c.y - s, c.x, c.y + s, 2.2f);
}
