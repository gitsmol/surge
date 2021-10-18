#include "PatchSelector.h"
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

#include "PatchSelector.h"
#include "SurgeStorage.h"
#include "SurgeGUIUtils.h"
#include "SurgeGUIEditor.h"
#include "RuntimeFont.h"
#include "UserDefaults.h"
#include "widgets/MenuCustomComponents.h"
#include "PatchDB.h"
#include "fmt/core.h"

namespace Surge
{
namespace Widgets
{

struct PatchDBTypeAheadProvider : public TypeAheadDataProvider
{
    SurgeStorage *storage;
    std::vector<PatchStorage::PatchDB::patchRecord> lastSearchResult;
    std::vector<int> searchFor(const std::string &s) override
    {
        lastSearchResult = storage->patchDB->queryFromQueryString(s);
        std::vector<int> res(lastSearchResult.size());
        std::iota(res.begin(), res.end(), 0);
        return res;
    }
    std::string textBoxValueForIndex(int idx) override
    {
        if (idx >= 0 && idx < lastSearchResult.size())
            return lastSearchResult[idx].name;
        return "<<ERROR>>";
    }

    int getRowHeight() override { return 23; }
    int getDisplayedRows() override { return 12; }

    juce::Colour rowBg{juce::Colours::white}, hlRowBg{juce::Colours::lightblue},
        rowText{juce::Colours::black}, hlRowText{juce::Colours::red},
        rowSubText{juce::Colours::orange}, hlRowSubText{juce::Colours::hotpink},
        divider{juce::Colours::orchid};

    void paintDataItem(int searchIndex, juce::Graphics &g, int width, int height,
                       bool rowIsSelected) override
    {
        g.fillAll(rowBg);
        if (rowIsSelected)
        {
            auto r = juce::Rectangle<int>(0, 0, width, height).reduced(2, 2);
            g.setColour(hlRowBg);
            g.fillRect(r);
        }

        g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(12));
        if (searchIndex >= 0 && searchIndex < lastSearchResult.size())
        {
            auto pr = lastSearchResult[searchIndex];
            auto r = juce::Rectangle<int>(4, 0, width - 8, height - 1);
            if (rowIsSelected)
                g.setColour(hlRowText);
            else
                g.setColour(rowText);
            g.drawText(pr.name, r, juce::Justification::centredTop);

            if (rowIsSelected)
                g.setColour(hlRowSubText);
            else
                g.setColour(rowSubText);
            g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(8));
            g.drawText(pr.cat, r, juce::Justification::bottomLeft);
            g.drawText(pr.author, r, juce::Justification::bottomRight);
        }
        g.setColour(divider);
        g.drawLine(4, height, width - 4, height, 1);
    }

    void paintOverChildren(juce::Graphics &g, const juce::Rectangle<int> &bounds) override
    {
        auto q = bounds.reduced(2, 2);
        g.setColour(rowText.withAlpha(0.5f));
        auto txt = fmt::format("{:d}", lastSearchResult.size());
        g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(8));
        g.drawText(txt, q, juce::Justification::topLeft);
    }
};

PatchSelector::PatchSelector()
{
    patchDbProvider = std::make_unique<PatchDBTypeAheadProvider>();
    typeAhead = std::make_unique<Surge::Widgets::TypeAhead>("patch select", patchDbProvider.get());
    typeAhead->setVisible(false);
    typeAhead->addTypeAheadListener(this);
    typeAhead->setToElementZeroOnReturn = true;
    addChildComponent(*typeAhead);
};
PatchSelector::~PatchSelector() = default;

void PatchSelector::setStorage(SurgeStorage *s)
{
    storage = s;
    storage->patchDB->initialize();
    patchDbProvider->storage = s;
}

void PatchSelector::paint(juce::Graphics &g)
{
    auto pbrowser = getLocalBounds();

    auto cat = getLocalBounds().withTrimmedLeft(3).withWidth(150).withHeight(getHeight() * 0.5);
    auto auth = getLocalBounds();

    if (skin->getVersion() >= 2)
    {
        cat = cat.translated(0, getHeight() * 0.5);
        auth = auth.withTrimmedRight(3)
                   .withWidth(150)
                   .translated(getWidth() - 150 - 3, 0)
                   .withTop(cat.getY())
                   .withHeight(cat.getHeight());
    }
    else
    {
        auth = cat;
        auth = auth.translated(0, pbrowser.getHeight() / 2);

        cat = cat.translated(0, 1);
        auth = auth.translated(0, -1);
    }

#if DEBUG_PATCH_AREAS
    g.setColour(juce::Colours::red);
    g.drawRect(pbrowser);
    g.drawRect(cat);
    g.drawRect(auth);
#endif

    // patch browser text color
    g.setColour(skin->getColor(Colors::PatchBrowser::Text));

    // patch name
    if (!isTypeaheadSearchOn)
    {
        g.setFont(Surge::GUI::getFontManager()->patchNameFont);
        g.drawText(pname, pbrowser, juce::Justification::centred);

        // category/author name
        g.setFont(Surge::GUI::getFontManager()->displayFont);
        g.drawText(category, cat, juce::Justification::centredLeft);
        g.drawText(author, auth,
                   skin->getVersion() >= 2 ? juce::Justification::centredRight
                                           : juce::Justification::centredLeft);
    }

    // favorites rect
    {
        juce::Graphics::ScopedSaveState gs(g);
        g.reduceClipRegion(favoritesRect);
        auto img = associatedBitmapStore->getImage(IDB_FAVORITE_BUTTON);
        int yShift = 13 * ((favoritesHover ? 1 : 0) + (isFavorite ? 2 : 0));
        img->drawAt(g, favoritesRect.getX(), favoritesRect.getY() - yShift, 1.0);
    }

    {
        juce::Graphics::ScopedSaveState gs(g);
        g.reduceClipRegion(searchRect);
        auto img = associatedBitmapStore->getImage(IDB_SEARCH_BUTTON);
        int yShift = 13 * ((searchHover ? 1 : 0));
        img->drawAt(g, searchRect.getX(), searchRect.getY() - yShift, 1.0);
    }
}

void PatchSelector::resized()
{
    auto fsize = 15;
    favoritesRect = getLocalBounds()
                        .withTrimmedBottom(getHeight() - fsize)
                        .withTrimmedLeft(getWidth() - fsize)
                        .reduced(1, 1)
                        .translated(-2, 1);
    searchRect = getLocalBounds()
                     .withTrimmedBottom(getHeight() - fsize)
                     .withWidth(fsize)
                     .reduced(1, 1)
                     .translated(2, 1);

    auto tad = getLocalBounds().reduced(fsize + 4, 0).translated(0, -2);
    typeAhead->setBounds(tad);
}

void PatchSelector::mouseEnter(const juce::MouseEvent &)
{
    if (tooltipCountdown < 0)
    {
        tooltipCountdown = 5;
        juce::Timer::callAfterDelay(200, [this]() { shouldTooltip(); });
    }
}

void PatchSelector::mouseMove(const juce::MouseEvent &e)
{
    if (tooltipCountdown >= 0)
        tooltipCountdown = 5;
    toggleCommentTooltip(false);
    auto pfh = favoritesHover;
    favoritesHover = false;
    if (favoritesRect.contains(e.position.toInt()))
    {
        favoritesHover = true;
    }

    auto psh = searchHover;
    searchHover = false;
    if (searchRect.contains(e.position.toInt()))
    {
        searchHover = true;
    }

    if (favoritesHover != pfh || searchHover != psh)
    {
        repaint();
    }
}
void PatchSelector::mouseDown(const juce::MouseEvent &e)
{
    if (e.mods.isMiddleButtonDown())
    {
        notifyControlModifierClicked(e.mods);
        return;
    }

    if (favoritesRect.contains(e.position.toInt()))
    {
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu menu;

            menu.addSectionHeader("FAVORITES");
            auto haveFavs = optionallyAddFavorites(menu, false, false);

            if (haveFavs)
            {
                menu.showMenuAsync(juce::PopupMenu::Options(), [this](int) { this->endHover(); });
            }

            return;
        }
        else
        {
            isFavorite = !isFavorite;
            auto sge = firstListenerOfType<SurgeGUIEditor>();

            if (sge)
            {
                sge->setPatchAsFavorite(isFavorite);
                repaint();
            }
        }
        return;
    }

    if (e.mods.isShiftDown() || searchRect.contains(e.position.toInt()))
    {
        toggleTypeAheadSearch(!isTypeaheadSearchOn);
        return;
    }

    // if RMB is down, only show the current category
    bool single_category = e.mods.isRightButtonDown() || e.mods.isCommandDown();
    showClassicMenu(single_category);
}

void PatchSelector::shouldTooltip()
{
    if (tooltipCountdown < 0)
    {
        return;
    }

    tooltipCountdown--;

    if (tooltipCountdown == 0)
    {
        tooltipCountdown = -1;
        toggleCommentTooltip(true);
    }
    else
    {
        juce::Timer::callAfterDelay(200, [this]() { shouldTooltip(); });
    }
}

void PatchSelector::toggleCommentTooltip(bool b)
{
    if (b && !comment.empty())
        std::cout << "-------------\n" << comment << "---------------\n" << std::endl;
}

void PatchSelector::openPatchBrowser()
{
    auto sge = firstListenerOfType<SurgeGUIEditor>();

    if (sge)
    {
        sge->showOverlay(SurgeGUIEditor::PATCH_BROWSER);
    }
}
void PatchSelector::showClassicMenu(bool single_category)
{
    auto contextMenu = juce::PopupMenu();
    int main_e = 0;
    bool has_3rdparty = false;
    int last_category = current_category;
    auto patch_cat_size = storage->patch_category.size();

    if (single_category)
    {
        /*
        ** in the init scenario we don't have a category yet. Our two choices are
        ** don't pop up the menu or pick one. I choose to pick one. If I can
        ** find the one called "Init" use that. Otherwise pick 0.
        */
        int rightMouseCategory = current_category;

        if (current_category < 0)
        {
            if (storage->patchCategoryOrdering.size() == 0)
            {
                return;
            }

            for (auto c : storage->patchCategoryOrdering)
            {
                if (_stricmp(storage->patch_category[c].name.c_str(), "Init") == 0)
                {
                    rightMouseCategory = c;
                }
            }

            if (rightMouseCategory < 0)
            {
                rightMouseCategory = storage->patchCategoryOrdering[0];
            }
        }

        // get just the category name and not the path leading to it
        std::string menuName = storage->patch_category[rightMouseCategory].name;

        if (menuName.find_last_of(PATH_SEPARATOR) != std::string::npos)
        {
            menuName = menuName.substr(menuName.find_last_of(PATH_SEPARATOR) + 1);
        }

        std::transform(menuName.begin(), menuName.end(), menuName.begin(), ::toupper);

        contextMenu.addSectionHeader("PATCHES (" + menuName + ")");

        populatePatchMenuForCategory(rightMouseCategory, contextMenu, single_category, main_e,
                                     false);
    }
    else
    {
        bool addedFavorites = false;
        if (patch_cat_size && storage->firstThirdPartyCategory > 0)
        {
            contextMenu.addSectionHeader("FACTORY PATCHES");
        }

        for (int i = 0; i < patch_cat_size; i++)
        {
            if (i == storage->firstThirdPartyCategory || i == storage->firstUserCategory)
            {
                std::string txt;
                bool favs = false;

                if (i == storage->firstThirdPartyCategory && storage->firstUserCategory != i)
                {
                    txt = "THIRD PARTY PATCHES";
                }
                else
                {
                    favs = true;
                    txt = "USER PATCHES";
                }

                contextMenu.addColumnBreak();
                contextMenu.addSectionHeader(txt);
                if (favs && optionallyAddFavorites(contextMenu, false))
                    contextMenu.addSeparator();
                addedFavorites = true;
            }

            // remap index to the corresponding category in alphabetical order.
            int c = storage->patchCategoryOrdering[i];

            populatePatchMenuForCategory(c, contextMenu, single_category, main_e, true);
        }
        if (!addedFavorites)
        {
            optionallyAddFavorites(contextMenu, true);
        }
    }

    contextMenu.addColumnBreak();
    contextMenu.addSectionHeader("FUNCTIONS");

    auto initAction = [this]() {
        int i = 0;

        bool lookingForFactory = (storage->initPatchCategoryType == "Factory");
        for (auto p : storage->patch_list)
        {
            if (p.name == storage->initPatchName &&
                storage->patch_category[p.category].name == storage->initPatchCategory &&
                storage->patch_category[p.category].isFactory == lookingForFactory)
            {
                loadPatch(i);
                break;
            }

            ++i;
        }
    };

    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Initialize Patch"), initAction);

    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Set Current Patch as Default"), [this]() {
        Surge::Storage::updateUserDefaultValue(storage, Surge::Storage::InitialPatchName,
                                               storage->patch_list[current_patch].name);

        Surge::Storage::updateUserDefaultValue(storage, Surge::Storage::InitialPatchCategory,
                                               storage->patch_category[current_category].name);

        Surge::Storage::updateUserDefaultValue(
            storage, Surge::Storage::InitialPatchCategoryType,
            storage->patch_category[current_category].isFactory ? "Factory" : "User");
    });

    contextMenu.addSeparator();

    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Save Patch"), [this]() {
        auto sge = firstListenerOfType<SurgeGUIEditor>();

        if (sge)
        {
            sge->showOverlay(SurgeGUIEditor::SAVE_PATCH);
        }
    });

    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Load Patch from File..."), [this]() {
        auto sge = firstListenerOfType<SurgeGUIEditor>();
        auto patchPath = storage->userPatchesPath;

        patchPath =
            Surge::Storage::getUserDefaultPath(storage, Surge::Storage::LastPatchPath, patchPath);

        if (!sge)
        {
            return;
        }

        sge->fileChooser = std::make_unique<juce::FileChooser>(
            "Select Patch to Load", juce::File(path_to_string(patchPath)), "*.fxp");

        sge->fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, patchPath](const juce::FileChooser &c) {
                auto ress = c.getResults();
                if (ress.size() != 1)
                    return;

                auto res = c.getResult();
                auto rString = res.getFullPathName().toStdString();
                auto sge = firstListenerOfType<SurgeGUIEditor>();

                if (sge)
                {
                    sge->queuePatchFileLoad(rString);
                }

                auto dir = string_to_path(res.getParentDirectory().getFullPathName().toStdString());

                if (dir != patchPath)
                {
                    Surge::Storage::updateUserDefaultPath(storage, Surge::Storage::LastPatchPath,
                                                          dir);
                }
            });
    });

    contextMenu.addSeparator();

    if (isUser)
    {
        contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Rename Patch"), [this]() {
            storage->reportError("This function has not been implemented yet!", "Coming Soon");
        });

        contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Delete Patch"), [this]() {
            storage->reportError("This function has not been implemented yet!", "Coming Soon");
        });
    }

    contextMenu.addSeparator();

#if INCLUDE_PATCH_BROWSER
    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Open Patch Database"), [this]() {
        auto sge = firstListenerOfType<SurgeGUIEditor>();

        if (sge)
        {
            sge->showOverlay(SurgeGUIEditor::PATCH_BROWSER);
        }
    });
#endif

    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Refresh Patch Browser"),
                        [this]() { this->storage->refresh_patchlist(); });

    contextMenu.addSeparator();

    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Open User Patches Folder..."),
                        [this]() { Surge::GUI::openFileOrFolder(this->storage->userPatchesPath); });

    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Open Factory Patches Folder..."), [this]() {
        Surge::GUI::openFileOrFolder(this->storage->datapath / "patches_factory");
    });

    contextMenu.addItem(
        Surge::GUI::toOSCaseForMenu("Open Third Party Patches Folder..."),
        [this]() { Surge::GUI::openFileOrFolder(this->storage->datapath / "patches_3rdparty"); });

    contextMenu.addSeparator();

    auto sge = firstListenerOfType<SurgeGUIEditor>();

    if (sge)
    {
        auto hu = sge->helpURLForSpecial("patch-browser");
        auto lurl = hu;

        if (hu != "")
        {
            lurl = sge->fullyResolvedHelpURL(hu);
        }

        auto hmen = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>("Patch Browser", lurl);

        hmen->setSkin(skin, associatedBitmapStore);
        hmen->setCenterBold(false);
        contextMenu.addCustomItem(-1, std::move(hmen));
    }

    auto o = juce::PopupMenu::Options();
    if (sge)
        o = sge->optionsForPosition(getBounds().getBottomLeft());
    contextMenu.showMenuAsync(o);
}

bool PatchSelector::optionallyAddFavorites(juce::PopupMenu &p, bool addColumnBreak,
                                           bool addToSubMenu)
{
    std::vector<std::pair<int, Patch>> favs;
    int i = 0;

    for (auto p : storage->patch_list)
    {
        if (p.isFavorite)
        {
            favs.emplace_back(i, p);
        }

        i++;
    }

    if (favs.empty())
    {
        return false;
    }

    std::sort(favs.begin(), favs.end(),
              [](const auto &a, const auto &b) { return a.second.name < b.second.name; });

    if (addColumnBreak)
    {
        p.addColumnBreak();
        p.addSectionHeader("FAVORITES");
    }

    if (addToSubMenu)
    {
        auto subMenu = juce::PopupMenu();

        subMenu.addSectionHeader("FAVORITES");

        for (auto f : favs)
        {
            subMenu.addItem(juce::CharPointer_UTF8(f.second.name.c_str()),
                            [this, f]() { this->loadPatch(f.first); });
        }

        p.addSubMenu("Favorites", subMenu);
    }
    else
    {
        for (auto f : favs)
        {
            p.addItem(juce::CharPointer_UTF8(f.second.name.c_str()),
                      [this, f]() { this->loadPatch(f.first); });
        }
    }

    return true;
}

bool PatchSelector::populatePatchMenuForCategory(int c, juce::PopupMenu &contextMenu,
                                                 bool single_category, int &main_e, bool rootCall)
{
    bool amIChecked = false;
    PatchCategory cat = storage->patch_category[c];

    // stop it going in the top menu which is a straight iteration
    if (rootCall && !cat.isRoot)
    {
        return false;
    }

    // don't do empty categories
    if (cat.numberOfPatchesInCategoryAndChildren == 0)
    {
        return false;
    }

    int splitcount = 256;

    // Go through the whole patch list in alphabetical order and filter
    // out only the patches that belong to the current category.
    std::vector<int> ctge;

    for (auto p : storage->patchOrdering)
    {
        if (storage->patch_list[p].category == c)
        {
            ctge.push_back(p);
        }
    }

    // Divide categories with more entries than splitcount into subcategories f.ex. bass (1, 2) etc
    int n_subc = 1 + (std::max(2, (int)ctge.size()) - 1) / splitcount;

    for (int subc = 0; subc < n_subc; subc++)
    {
        std::string name;
        juce::PopupMenu availMenu;
        juce::PopupMenu *subMenu;

        if (single_category)
        {
            subMenu = &contextMenu;
        }
        else
        {
            subMenu = &availMenu;
        }

        int sub = 0;

        for (int i = subc * splitcount; i < std::min((subc + 1) * splitcount, (int)ctge.size());
             i++)
        {
            int p = ctge[i];

            name = storage->patch_list[p].name;

            bool thisCheck = false;

            if (p == current_patch)
            {
                thisCheck = true;
                amIChecked = true;
            }

            subMenu->addItem(name, true, thisCheck, [this, p]() { this->loadPatch(p); });
            sub++;

            if (sub != 0 && sub % 32 == 0)
            {
                subMenu->addColumnBreak();

                if (single_category)
                {
                    subMenu->addSectionHeader("");
                }
            }
        }

        for (auto &childcat : cat.children)
        {
            // this isn't the best approach but it works
            int idx = 0;

            for (auto &cc : storage->patch_category)
            {
                if (cc.name == childcat.name && cc.internalid == childcat.internalid)
                {
                    break;
                }

                idx++;
            }

            bool checkedKid = populatePatchMenuForCategory(idx, *subMenu, false, main_e, false);

            if (checkedKid)
            {
                amIChecked = true;
            }
        }

        // get just the category name and not the path leading to it
        std::string menuName = storage->patch_category[c].name;

        if (menuName.find_last_of(PATH_SEPARATOR) != std::string::npos)
        {
            menuName = menuName.substr(menuName.find_last_of(PATH_SEPARATOR) + 1);
        }

        if (n_subc > 1)
        {
            name = menuName.c_str() + (subc + 1);
        }
        else
        {
            name = menuName.c_str();
        }

        if (!single_category)
        {
            contextMenu.addSubMenu(name, *subMenu, true, nullptr, amIChecked);
        }

        main_e++;
    }

    return amIChecked;
}

void PatchSelector::loadPatch(int id)
{
    if (id >= 0)
    {
        enqueue_sel_id = id;
        notifyValueChanged();
    }
}

int PatchSelector::getCurrentPatchId() const { return current_patch; }

int PatchSelector::getCurrentCategoryId() const { return current_category; }

void PatchSelector::itemSelected(int providerIndex)
{
    auto sr = patchDbProvider->lastSearchResult[providerIndex];
    auto sge = firstListenerOfType<SurgeGUIEditor>();
    toggleTypeAheadSearch(false);
    if (sge)
    {
        sge->queuePatchFileLoad(sr.file);
    }
}
void PatchSelector::typeaheadCanceled() { toggleTypeAheadSearch(false); }

void PatchSelector::onSkinChanged()
{
    auto transBlack = juce::Colours::black.withAlpha(0.f);
    typeAhead->setColour(juce::TextEditor::outlineColourId, transBlack);
    typeAhead->setColour(juce::TextEditor::backgroundColourId, transBlack);
    typeAhead->setColour(juce::TextEditor::focusedOutlineColourId, transBlack);
    typeAhead->setFont(Surge::GUI::getFontManager()->patchNameFont);

    typeAhead->setColour(juce::TextEditor::textColourId,
                         skin->getColor(Colors::Dialog::Entry::Text));
    typeAhead->setColour(juce::TextEditor::highlightedTextColourId,
                         skin->getColor(Colors::Dialog::Entry::Text));
    typeAhead->setColour(juce::TextEditor::highlightColourId,
                         skin->getColor(Colors::Dialog::Entry::Focus));

    typeAhead->setColour(Surge::Widgets::TypeAhead::ColourIds::borderid,
                         skin->getColor(Colors::PatchBrowser::TypeAheadList::Border));
    typeAhead->setColour(Surge::Widgets::TypeAhead::ColourIds::emptyBackgroundId,
                         skin->getColor(Colors::PatchBrowser::TypeAheadList::Background));

    patchDbProvider->rowBg = skin->getColor(Colors::PatchBrowser::TypeAheadList::Background);
    patchDbProvider->rowText = skin->getColor(Colors::PatchBrowser::TypeAheadList::Text);
    patchDbProvider->hlRowBg =
        skin->getColor(Colors::PatchBrowser::TypeAheadList::HighlightBackground);
    patchDbProvider->hlRowText = skin->getColor(Colors::PatchBrowser::TypeAheadList::HighlightText);
    patchDbProvider->rowSubText = skin->getColor(Colors::PatchBrowser::TypeAheadList::SubText);
    patchDbProvider->hlRowSubText =
        skin->getColor(Colors::PatchBrowser::TypeAheadList::HighlightSubText);
    patchDbProvider->divider = skin->getColor(Colors::PatchBrowser::TypeAheadList::Divider);
}

void PatchSelector::toggleTypeAheadSearch(bool b)
{
    isTypeaheadSearchOn = b;
    if (isTypeaheadSearchOn)
    {
        storage->initializePatchDb();
        bool enable = true;
        auto txt = pname;
        if (storage->patchDB->numberOfJobsOutstanding() > 0)
        {
            enable = false;
            txt = "Updating Patch DB: " +
                  std::to_string(storage->patchDB->numberOfJobsOutstanding()) + " jobs";
        }
        typeAhead->setJustification(juce::Justification::centred);
        typeAhead->setText(txt, juce::NotificationType::dontSendNotification);
        typeAhead->setIndents(4, (typeAhead->getHeight() - typeAhead->getTextHeight()) / 2);

        typeAhead->setVisible(true);
        typeAhead->setEnabled(enable);

        typeAhead->grabKeyboardFocus();
        typeAhead->selectAll();

        if (!enable)
        {
            juce::Timer::callAfterDelay(1000 / 30, [this]() { this->enableTypeAheadIfReady(); });
        }
    }
    else
    {
        typeAhead->setVisible(false);
    }
    repaint();
    return;
}

void PatchSelector::enableTypeAheadIfReady()
{
    if (!isTypeaheadSearchOn)
        return;

    bool enable = true;
    auto txt = pname;
    if (storage->patchDB->numberOfJobsOutstanding() > 0)
    {
        enable = false;
        txt = "Updating Patch DB: " + std::to_string(storage->patchDB->numberOfJobsOutstanding()) +
              " jobs";
    }
    typeAhead->setText(txt, juce::NotificationType::dontSendNotification);
    typeAhead->setEnabled(enable);

    if (enable)
    {
        typeAhead->grabKeyboardFocus();
        typeAhead->selectAll();
    }
    else
    {
        juce::Timer::callAfterDelay(1000 / 30, [this]() { this->enableTypeAheadIfReady(); });
    }
}

#if SURGE_JUCE_ACCESSIBLE
class PatchSelectorAH : public juce::AccessibilityHandler
{
  public:
    explicit PatchSelectorAH(PatchSelector *sel)
        : selector(sel), juce::AccessibilityHandler(
                             *sel, juce::AccessibilityRole::label,
                             juce::AccessibilityActions()
                                 .addAction(juce::AccessibilityActionType::press,
                                            [sel] { sel->openPatchBrowser(); })
                                 .addAction(juce::AccessibilityActionType::showMenu,
                                            [sel] { sel->showClassicMenu(); }),
                             {std::make_unique<PatchSelectorValueInterface>(sel)})
    {
    }

  private:
    class PatchSelectorValueInterface : public juce::AccessibilityTextValueInterface
    {
      public:
        explicit PatchSelectorValueInterface(PatchSelector *sel) : selector(sel) {}

        bool isReadOnly() const override { return true; }
        juce::String getCurrentValueAsString() const override
        {
            return selector->getPatchNameAccessibleValue();
        }
        void setValueAsString(const juce::String &) override {}

      private:
        PatchSelector *selector;

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchSelectorValueInterface)
    };

    PatchSelector *selector;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchSelectorAH)
};

std::unique_ptr<juce::AccessibilityHandler> PatchSelector::createAccessibilityHandler()
{
    return std::make_unique<PatchSelectorAH>(this);
}
#endif

} // namespace Widgets
} // namespace Surge