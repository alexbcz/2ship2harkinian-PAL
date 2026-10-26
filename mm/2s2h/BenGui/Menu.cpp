#include "Menu.h"
#include "UIWidgets.hpp"
#include "BenPort.h"
#include "BenInputEditorWindow.h"
#include <ship/window/gui/GuiElement.h>
#include "BenModals.h"
#include "Notification.h"
#include <variant>
#include <spdlog/fmt/fmt.h>
#include "variables.h"
#include <algorithm>
#include <tuple>
#include <ship/config/Config.h>

#if defined(__ANDROID__)
#include <SDL.h>
#include <jni.h>
#endif

#include <fast/Fast3dWindow.h>

extern "C" {
#include "z64.h"
#include "functions.h"
extern PlayState* gPlayState;
}
std::vector<ImVec2> windowTypeSizes = { {} };

extern std::unordered_map<s16, const char*> warpPointSceneList;
extern void Warp();

namespace BenGui {
extern std::shared_ptr<BenModalWindow> mModalWindow;
}
std::vector<SearchWidget> extraSearchWidgets = {};

namespace Ship {
std::string disabledTempTooltip;
const char* disabledTooltip;
bool disabledValue = false;

bool operator==(Color_RGB8 const& l, Color_RGB8 const& r) noexcept {
    return l.r == r.r && l.g == r.g && l.b == r.b;
}

bool operator==(Color_RGBA8 const& l, Color_RGBA8 const& r) noexcept {
    return l.r == r.r && l.g == r.g && l.b == r.b && l.a == r.a;
}

bool operator<(Color_RGB8 const& l, Color_RGB8 const& r) noexcept {
    return (l.r < r.r && l.g <= r.g && l.b <= r.b) || (l.r <= r.r && l.g < r.g && l.b <= r.b) ||
           (l.r <= r.r && l.g <= r.g && l.b < r.b);
}

bool operator<(Color_RGBA8 const& l, Color_RGBA8 const& r) noexcept {
    return (l.r < r.r && l.g <= r.g && l.b <= r.b && l.a <= r.a) ||
           (l.r <= r.r && l.g < r.g && l.b <= r.b && l.a <= r.a) ||
           (l.r <= r.r && l.g <= r.g && l.b < r.b && l.a <= r.a) ||
           (l.r <= r.r && l.g <= r.g && l.b <= r.b && l.a < r.a);
}

bool operator>(Color_RGB8 const& l, Color_RGB8 const& r) noexcept {
    return (l.r > r.r && l.g >= r.g && l.b >= r.b) || (l.r >= r.r && l.g > r.g && l.b >= r.b) ||
           (l.r >= r.r && l.g >= r.g && l.b > r.b);
}

bool operator>(Color_RGBA8 const& l, Color_RGBA8 const& r) noexcept {
    return (l.r > r.r && l.g >= r.g && l.b >= r.b && l.a >= r.a) ||
           (l.r >= r.r && l.g > r.g && l.b >= r.b && l.a >= r.a) ||
           (l.r >= r.r && l.g >= r.g && l.b > r.b && l.a >= r.a) ||
           (l.r >= r.r && l.g >= r.g && l.b >= r.b && l.a > r.a);
}

uint32_t GetVectorIndexOf(std::vector<std::string>& vector, std::string value) {
    return std::distance(vector.begin(), std::find(vector.begin(), vector.end(), value));
}

#if defined(__ANDROID__)
// An ImGui InputText can stay active after Android's Back gesture hides the IME.
// Re-request the real SDL text editor on a *new tap*, not on every frame with
// WantTextInput=true, so users can hide the keyboard to inspect search results.
static void AndroidRequestSearchKeyboard() {
    // Also ensures SDL receives SDL_TEXTINPUT events when the field is opened.
    SDL_StartTextInput();

    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (env == nullptr || activity == nullptr) {
        return;
    }

    jclass activityClass = env->GetObjectClass(activity);
    if (activityClass != nullptr) {
        jmethodID requestMethod = env->GetMethodID(activityClass, "requestSearchKeyboardFromNative", "()V");
        if (requestMethod != nullptr) {
            env->CallVoidMethod(activity, requestMethod);
        }
        env->DeleteLocalRef(activityClass);
    }
    env->DeleteLocalRef(activity);
}

// Mobile drag-scroll is resolved once after all child windows have been drawn.
// This also covers widgets drawing their own nested BeginChild() windows.
static constexpr float kAndroidMenuDragThreshold = 18.0f;
static void AndroidMenuScrollWindowTree(ImGuiWindow* menuRoot) {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiContext& g = *GImGui;
    constexpr int button = ImGuiMouseButton_Left;
    static ImGuiID targetWindowId = 0;
    static bool directionDecided = false;
    static bool verticalGesture = false;

    if (!io.MouseDown[button]) {
        targetWindowId = 0;
        directionDecided = false;
        verticalGesture = false;
        return;
    }

    if (io.MouseClicked[button]) {
        targetWindowId = 0;
        directionDecided = false;
        verticalGesture = false;
        const ImVec2 origin = io.MouseClickedPos[button];
        int bestDepth = -1;
        // A child window can be scrollable even when its parent isn't.
        for (ImGuiWindow* candidate : g.Windows) {
            if (!candidate->Active || candidate->ScrollMax.y <= 0.0f ||
                (candidate->Flags & ImGuiWindowFlags_NoMouseInputs) ||
                !candidate->ClipRect.Contains(origin) ||
                !ImRect(candidate->Pos, candidate->Pos + candidate->Size).Contains(origin)) {
                continue;
            }
            int depth = 0;
            ImGuiWindow* parent = candidate;
            while (parent != nullptr && parent != menuRoot) {
                parent = parent->ParentWindow;
                depth++;
            }
            if (parent == menuRoot && depth > bestDepth) {
                targetWindowId = candidate->ID;
                bestDepth = depth;
            }
        }
        if (ImGuiWindow* target = ImGui::FindWindowByID(targetWindowId)) {
            // Preserve the native drag behavior of scrollbar handles.
            if (target->ScrollbarY &&
                origin.x >= target->Pos.x + target->Size.x - ImGui::GetStyle().ScrollbarSize) {
                targetWindowId = 0;
            }
        }
    }

    if (targetWindowId == 0) {
        return;
    }
    const ImVec2 travel = io.MousePos - io.MouseClickedPos[button];
    if (!directionDecided &&
        travel.x * travel.x + travel.y * travel.y >=
            kAndroidMenuDragThreshold * kAndroidMenuDragThreshold) {
        directionDecided = true;
        // Match the slider gesture classification: moderate diagonals should
        // stay horizontal, whereas clearly vertical swipes scroll the menu.
        constexpr float verticalDominance = 1.5f;
        verticalGesture = (travel.y * travel.y >
                           travel.x * travel.x * verticalDominance * verticalDominance);
    }
    if (!directionDecided || !verticalGesture) {
        return;
    }

    ImGuiWindow* target = ImGui::FindWindowByID(targetWindowId);
    if (target == nullptr || !target->Active) {
        return;
    }
    // A confirmed vertical swipe should scroll even when it began on a slider
    // or another active widget. Horizontal gestures still belong to widgets.
    // Sidebar entries already allow scrolling without releasing their active ID.
    const bool sidebar = std::string(target->Name).find(" Section") != std::string::npos;
    if (!sidebar && g.ActiveId != 0 && g.ActiveIdWindow != nullptr &&
        g.ActiveId != g.ActiveIdWindow->MoveId) {
        for (const ImGuiWindow* active = g.ActiveIdWindow; active != nullptr; active = active->ParentWindow) {
            if (active == target) {
                // Preserve intentional drag-and-drop, e.g. rearranging tracker items.
                if (g.DragDropActive) {
                    return;
                }
                ImGui::ClearActiveID();
                break;
            }
        }
    }
    ImGui::SetScrollY(target, target->Scroll.y - io.MouseDelta.y);
}
#endif

void Menu::InsertSidebarSearch() {
    menuEntries["Settings"].sidebars.emplace("Search", searchSidebarEntry);
    uint32_t curIndex = 0;
    if (!Ship_IsCStringEmpty(CVarGetString(menuEntries["Settings"].sidebarCvar, ""))) {
        curIndex = GetVectorIndexOf(menuEntries["Settings"].sidebarOrder,
                                    CVarGetString(menuEntries["Settings"].sidebarCvar, ""));
    }
    menuEntries["Settings"].sidebarOrder.insert(menuEntries["Settings"].sidebarOrder.begin() + searchSidebarIndex,
                                                "Search");
    if (curIndex > searchSidebarIndex) {
        CVarSetString(menuEntries["Settings"].sidebarCvar, menuEntries["Settings"].sidebarOrder.at(curIndex).c_str());
    }
}

void Menu::RemoveSidebarSearch() {
    uint32_t curIndex =
        GetVectorIndexOf(menuEntries["Settings"].sidebarOrder, CVarGetString(menuEntries["Settings"].sidebarCvar, ""));
    menuEntries["Settings"].sidebars.erase("Search");
    std::erase_if(menuEntries["Settings"].sidebarOrder, [](std::string& name) { return name == "Search"; });
    if (curIndex > searchSidebarIndex) {
        curIndex--;
    } else if (curIndex >= menuEntries["Settings"].sidebarOrder.size()) {
        curIndex = menuEntries["Settings"].sidebarOrder.size() - 1;
    }
    CVarSetString(menuEntries["Settings"].sidebarCvar, menuEntries["Settings"].sidebarOrder.at(curIndex).c_str());
}

void Menu::UpdateWindowBackendObjects() {
    int32_t runningWindowBackend = Ship::Context::GetRawInstance()->GetWindow()->GetWindowBackend();
    int32_t configWindowBackendId = Ship::Context::GetRawInstance()->GetConfig()->GetInt("Window.Backend.Id", -1);
    if (Ship::Context::GetRawInstance()->GetWindow()->IsAvailableWindowBackend(configWindowBackendId)) {
        configWindowBackend = static_cast<Fast::WindowBackend>(configWindowBackendId);
    } else {
        configWindowBackend = static_cast<Fast::WindowBackend>(runningWindowBackend);
    }

    availableWindowBackends = Ship::Context::GetRawInstance()->GetWindow()->GetAvailableWindowBackends();
    for (auto& backend : *availableWindowBackends) {
        availableWindowBackendsMap[backend] = windowBackendsMap.at(backend);
    }
}

bool Menu::IsMenuPopped() {
    return popped;
}

UIWidgets::Colors Menu::GetMenuThemeColor() {
    return menuThemeIndex;
}

Menu::Menu(const std::string& cVar, const std::string& name, uint8_t searchSidebarIndex_,
           UIWidgets::Colors defaultThemeIndex_)
    : GuiWindow(cVar, name), searchSidebarIndex(searchSidebarIndex_), defaultThemeIndex(defaultThemeIndex_) {
}

void Menu::InitElement() {
    popped = CVarGetInteger("gSettings.Menu.Popout", 0);
    poppedSize.x = CVarGetInteger("gSettings.Menu.PoppedWidth", 1280);
    poppedSize.y = CVarGetInteger("gSettings.Menu.PoppedHeight", 800);
    poppedPos.x = CVarGetInteger("gSettings.Menu.PoppedPos.x", 0);
    poppedPos.y = CVarGetInteger("gSettings.Menu.PoppedPos.y", 0);

    UpdateWindowBackendObjects();
}

void Menu::UpdateElement() {
    menuThemeIndex = static_cast<UIWidgets::Colors>(CVarGetInteger("gSettings.Menu.Theme", defaultThemeIndex));
}

bool ModernMenuSidebarEntry(std::string label) {
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec2 pos = window->DC.CursorPos;
    const ImGuiID sidebarId = window->GetID(std::string(label + "##Sidebar").c_str());
    ImVec2 labelSize = ImGui::CalcTextSize(label.c_str(), ImGui::FindRenderedTextEnd(label.c_str()), true);
    pos.y += style.FramePadding.y;
    pos.x = window->WorkRect.GetCenter().x - labelSize.x / 2;
    ImRect bb = { pos - style.FramePadding, pos + labelSize + style.FramePadding };
    ImGui::ItemSize(bb, style.FramePadding.y);
    ImGui::ItemAdd(bb, sidebarId);
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, sidebarId, &hovered, &held);
#if defined(__ANDROID__)
    // A swipe over a category must not switch sections on finger release.
    if (pressed && ImGui::GetIO().MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] >
                       kAndroidMenuDragThreshold * kAndroidMenuDragThreshold) {
        pressed = false;
    }
#endif
    if (pressed) {
        ImGui::MarkItemEdited(sidebarId);
    }
    window->DrawList->AddRectFilled(pos - style.FramePadding, pos + labelSize + style.FramePadding,
                                    ImGui::GetColorU32((held && hovered) ? ImGuiCol_ButtonActive
                                                       : hovered         ? ImGuiCol_ButtonHovered
                                                                         : ImGuiCol_Button),
                                    3.0f);
    UIWidgets::RenderText(pos, label.c_str(), ImGui::FindRenderedTextEnd(label.c_str()), true);
    return pressed;
}

bool ModernMenuHeaderEntry(std::string label) {
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec2 pos = window->DC.CursorPos;
    const ImGuiID headerId = window->GetID(std::string(label + "##Header").c_str());
    ImVec2 labelSize = ImGui::CalcTextSize(label.c_str(), ImGui::FindRenderedTextEnd(label.c_str()), true);
    ImRect bb = { pos, pos + labelSize + style.FramePadding * 2 };
    ImGui::ItemSize(bb, style.FramePadding.y);
    ImGui::ItemAdd(bb, headerId);
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, headerId, &hovered, &held);
    window->DrawList->AddRectFilled(bb.Min, bb.Max,
                                    ImGui::GetColorU32((held && hovered) ? ImGuiCol_ButtonActive
                                                       : hovered         ? ImGuiCol_ButtonHovered
                                                                         : ImGuiCol_Button),
                                    3.0f);
    pos += style.FramePadding;
    UIWidgets::RenderText(pos, label.c_str(), ImGui::FindRenderedTextEnd(label.c_str()), true);
    return pressed;
}

uint32_t Menu::DrawSearchResults(std::string& menuSearchText) {
    auto menuThemeIndex = static_cast<UIWidgets::Colors>(CVarGetInteger("gSettings.Menu.Theme", defaultThemeIndex));
#if !defined(__ANDROID__)
    // Desktop retains its nested search-results child. On Android, render into
    // the already-scrollable settings child so touch scrolling has one owner.
    ImGui::BeginChild("Search Results");
#endif
    int searchCount = 0;
    for (auto& menuLabel : menuOrder) {
        auto& menuEntry = menuEntries.at(menuLabel);
        for (auto& sidebarLabel : menuEntry.sidebarOrder) {
            auto& sidebar = menuEntry.sidebars[sidebarLabel];
            for (size_t i = 0; i < sidebar.columnWidgets.size(); i++) {
                auto& column = sidebar.columnWidgets.at(i);
                for (auto& info : column) {
                    if (info.type == WIDGET_SEARCH || info.type == WIDGET_SEPARATOR ||
                        info.type == WIDGET_SEPARATOR_TEXT || info.isHidden || info.hideInSearch) {
                        continue;
                    }
                    const char* tooltip = info.options->tooltip;
                    std::string widgetStr = std::string(info.name) + std::string(tooltip != nullptr ? tooltip : "");
                    std::transform(menuSearchText.begin(), menuSearchText.end(), menuSearchText.begin(), ::tolower);
                    menuSearchText.erase(std::remove(menuSearchText.begin(), menuSearchText.end(), ' '),
                                         menuSearchText.end());
                    std::transform(widgetStr.begin(), widgetStr.end(), widgetStr.begin(), ::tolower);
                    widgetStr.erase(std::remove(widgetStr.begin(), widgetStr.end(), ' '), widgetStr.end());
                    if (widgetStr.find(menuSearchText) != std::string::npos) {
                        MenuDrawItem(info, 90 / sidebar.columnCount, menuThemeIndex);
                        ImGui::PushStyleColor(ImGuiCol_Text, UIWidgets::ColorValues.at(UIWidgets::Colors::Gray));
                        std::string origin = fmt::format("  ({} -> {}, Col {})", menuEntry.label, sidebarLabel, i + 1);
                        ImGui::Text("%s", origin.c_str());
                        ImGui::PopStyleColor();
                        searchCount++;
                    }
                }
            }
        }
    }
    for (auto& entry : extraSearchWidgets) {
        if (entry.info.type == WIDGET_SEARCH || entry.info.type == WIDGET_SEPARATOR ||
            entry.info.type == WIDGET_SEPARATOR_TEXT || entry.info.isHidden || entry.info.hideInSearch) {
            continue;
        }
        std::string widgetStr = entry.info.name + entry.info.options->tooltip + entry.extraTerms + entry.sidebarName;
        std::transform(widgetStr.begin(), widgetStr.end(), widgetStr.begin(), ::tolower);
        widgetStr.erase(std::remove(widgetStr.begin(), widgetStr.end(), ' '), widgetStr.end());
        if (widgetStr.find(menuSearchText) != std::string::npos) {
            MenuDrawItem(entry.info, 400, menuThemeIndex);
            ImGui::PushStyleColor(ImGuiCol_Text, UIWidgets::ColorValues.at(UIWidgets::Colors::Gray));
            std::string origin = fmt::format("  ({} -> {}, {})", entry.menuName, entry.sidebarName, entry.location);
            ImGui::Text("%s", origin.c_str());
            ImGui::PopStyleColor();
            searchCount++;
        }
    }
#if defined(__ANDROID__)
    // This window receives a scroll-to-top target (0) every frame while
    // rendering search results. It is applied by ImGui on the following frame,
    // snapping back after the user stops dragging the list or its scrollbar.
    ImGuiWindow* resultsWindow = ImGui::GetCurrentWindow();
    static std::string previousQuery;
    static ImGuiID previousWindowId = 0;
    static int previousFrame = -1;
    const int frame = ImGui::GetFrameCount();
    const bool searchChanged = previousQuery != menuSearchText || previousWindowId != resultsWindow->ID ||
                               (previousFrame >= 0 && frame > previousFrame + 1);
    if (searchChanged) {
        ImGui::SetScrollY(resultsWindow, 0.0f);
    } else if (resultsWindow->Scroll.y > 0.0f && resultsWindow->ScrollTarget.y == 0.0f) {
        resultsWindow->ScrollTarget.y = FLT_MAX; // No pending scroll request
    }
    previousQuery = menuSearchText;
    previousWindowId = resultsWindow->ID;
    previousFrame = frame;
#endif
    return searchCount;
}

void Menu::AddMenuEntry(std::string entryName, const char* entryCvar) {
    menuEntries.emplace(entryName, MainMenuEntry{ entryName, entryCvar });
    menuOrder.push_back(entryName);
}

void Menu::AddSearchWidget(SearchWidget widget) {
    extraSearchWidgets.push_back(widget);
}

std::unordered_map<uint32_t, disabledInfo>& Menu::GetDisabledMap() {
    return disabledMap;
}

#if defined(__ANDROID__)
// Wrap checkbox labels using the actual rendered width, not the desktop
// section's historical character-count limit. Explicit newlines are kept.
static std::string AndroidWrapCheckboxLabel(const std::string& label) {
    const size_t idOffset = label.find("##");
    const std::string visible = label.substr(0, idOffset);
    const ImGuiStyle& style = ImGui::GetStyle();
    const float available = ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() -
                            style.ItemInnerSpacing.x * 2.0f - style.FramePadding.x * 2.0f;
    if (available <= ImGui::GetFontSize() * 3.0f ||
        (visible.find('\n') == std::string::npos && ImGui::CalcTextSize(visible.c_str()).x <= available)) {
        return label;
    }

    std::string wrapped;
    std::string line;
    std::string word;
    // Wrap at word boundaries; never split a word just to meet a character limit.
    // The source's intentional newline characters remain hard line breaks.
    for (size_t i = 0; i <= visible.size(); ++i) {
        const char ch = i < visible.size() ? visible[i] : '\0';
        if (ch == ' ' || ch == '\n' || ch == '\0') {
            if (!word.empty()) {
                const std::string candidate = line.empty() ? word : line + " " + word;
                if (!line.empty() && ImGui::CalcTextSize(candidate.c_str()).x > available) {
                    wrapped += line + "\n";
                    line = word;
                } else {
                    line = candidate;
                }
                word.clear();
            }
            if (ch == '\n') {
                wrapped += line + "\n";
                line.clear();
            }
        } else {
            word += ch;
        }
    }
    wrapped += line;
    if (idOffset != std::string::npos) {
        wrapped += label.substr(idOffset); // Keep the original ImGui widget ID.
    }
    return wrapped;
}
#endif

void Menu::MenuDrawItem(WidgetInfo& widget, uint32_t width, UIWidgets::Colors menuThemeIndex) {
    disabledTempTooltip = "This setting is disabled because: \n\n";
    disabledValue = false;
    disabledTooltip = " ";

    if (widget.preFunc != nullptr) {
        widget.ResetDisables();
        widget.preFunc(widget);
        if (widget.isHidden) {
            return;
        }
        if (!widget.activeDisables.empty()) {
            widget.options->disabled = true;
            for (auto option : widget.activeDisables) {
                disabledTempTooltip += std::string("- ") + disabledMap.at(option).reason + std::string("\n");
            }
            widget.options->disabledTooltip = disabledTempTooltip.c_str();
        }
    }

    if (widget.sameLine) {
        ImGui::SameLine();
    }

    try {
        switch (widget.type) {
            case WIDGET_CHECKBOX: {
                bool* pointer = std::get<bool*>(widget.valuePointer);
                if (pointer == nullptr) {
                    SPDLOG_ERROR("Checkbox Widget requires a value pointer, currently nullptr");
                    assert(false);
                    return;
                }
                auto options = std::static_pointer_cast<UIWidgets::CheckboxOptions>(widget.options);
                options->color = menuThemeIndex;
#if defined(__ANDROID__)
                const std::string label = AndroidWrapCheckboxLabel(widget.name);
#else
                const std::string label = UIWidgets::WrappedText(widget.name.c_str(), width);
#endif
                if (UIWidgets::Checkbox(label.c_str(), pointer, *options)) {
                    if (widget.callback != nullptr) {
                        widget.callback(widget);
                    }
                }
            } break;
            case WIDGET_CVAR_CHECKBOX: {
#if defined(__ANDROID__)
                if (widget.name == "Popout Menu") {
                    break;
                }
#endif
                auto options = std::static_pointer_cast<UIWidgets::CheckboxOptions>(widget.options);
                options->color = menuThemeIndex;
#if defined(__ANDROID__)
                const std::string label = AndroidWrapCheckboxLabel(widget.name);
#else
                const std::string label = UIWidgets::WrappedText(widget.name.c_str(), width);
#endif
                if (UIWidgets::CVarCheckbox(label.c_str(), widget.cVar, *options)) {
                    if (widget.callback != nullptr) {
                        widget.callback(widget);
                    }
                };
            } break;
            case WIDGET_AUDIO_BACKEND: {
                auto currentAudioBackend = Ship::Context::GetRawInstance()->GetAudio()->GetCurrentAudioBackend();
                UIWidgets::ComboboxOptions options = {};
                options.color = menuThemeIndex;
                options.tooltip = "Sets the audio API used by the game. Requires a relaunch to take effect.";
                options.disabled =
                    Ship::Context::GetRawInstance()->GetAudio()->GetAvailableAudioBackends()->size() <= 1;
                options.disabledTooltip = "Only one audio API is available on this platform.";
                if (UIWidgets::Combobox("Audio API", &currentAudioBackend, &audioBackendsMap, options)) {
                    Ship::Context::GetRawInstance()->GetAudio()->SetCurrentAudioBackend(currentAudioBackend);
                }
            } break;
            case WIDGET_VIDEO_BACKEND: {
                UIWidgets::ComboboxOptions options = {};
                options.color = menuThemeIndex;
                options.tooltip = "Sets the renderer API used by the game.";
                options.disabled = availableWindowBackends->size() <= 1;
                options.disabledTooltip = "Only one renderer API is available on this platform.";
                if (UIWidgets::Combobox("Renderer API (Needs reload)", &configWindowBackend,
                                        &availableWindowBackendsMap, options)) {
                    Ship::Context::GetRawInstance()->GetConfig()->SetInt("Window.Backend.Id", configWindowBackend);
                    Ship::Context::GetRawInstance()->GetConfig()->SetString("Window.Backend.Name",
                                                                            windowBackendsMap.at(configWindowBackend));
                    Ship::Context::GetRawInstance()->GetConfig()->Save();
                    UpdateWindowBackendObjects();
                }
            } break;
            case WIDGET_SEPARATOR: {
                ImGui::Separator();
            } break;
            case WIDGET_SEPARATOR_TEXT: {
                if (widget.options->color != UIWidgets::Colors::NoColor) {
                    ImGui::PushStyleColor(ImGuiCol_Text, UIWidgets::ColorValues.at(widget.options->color));
                }
                ImGui::SeparatorText(widget.name.c_str());
                if (widget.options->color != UIWidgets::Colors::NoColor) {
                    ImGui::PopStyleColor();
                }
            } break;
            case WIDGET_TEXT: {
                if (widget.options->color != UIWidgets::Colors::NoColor) {
                    ImGui::PushStyleColor(ImGuiCol_Text, UIWidgets::ColorValues.at(widget.options->color));
                }
                ImGui::AlignTextToFramePadding();
                ImGui::TextWrapped("%s", widget.name.c_str());
                if (widget.options->color != UIWidgets::Colors::NoColor) {
                    ImGui::PopStyleColor();
                }
            } break;
            case WIDGET_COMBOBOX: {
                int32_t* pointer = std::get<int32_t*>(widget.valuePointer);
                if (pointer == nullptr) {
                    SPDLOG_ERROR("Combobox Widget requires a value pointer, currently nullptr");
                    assert(false);
                    return;
                }
                auto options = std::static_pointer_cast<UIWidgets::ComboboxOptions>(widget.options);
                options->color = menuThemeIndex;
                bool result = false;
                if (std::holds_alternative<UIWidgets::ComboVec_t>(options->comboVariant)) {
                    result = UIWidgets::Combobox(widget.name.c_str(), pointer,
                                                 *std::get<UIWidgets::ComboVec_t>(options->comboVariant), *options);
                } else if (std::holds_alternative<UIWidgets::ComboMap_t>(options->comboVariant)) {
                    result = UIWidgets::Combobox(widget.name.c_str(), pointer,
                                                 std::get<UIWidgets::ComboMap_t>(options->comboVariant), *options);
                }
                if (result) {
                    if (widget.callback != nullptr) {
                        widget.callback(widget);
                    }
                };
            } break;
            case WIDGET_CVAR_COMBOBOX: {
                auto options = std::static_pointer_cast<UIWidgets::ComboboxOptions>(widget.options);
                options->color = menuThemeIndex;
                bool result = false;
                if (std::holds_alternative<UIWidgets::ComboVec_t>(options->comboVariant)) {
                    result = UIWidgets::CVarCombobox(widget.name.c_str(), widget.cVar,
                                                     *std::get<UIWidgets::ComboVec_t>(options->comboVariant), *options);
                } else if (std::holds_alternative<UIWidgets::ComboMap_t>(options->comboVariant)) {
                    result = UIWidgets::CVarCombobox(widget.name.c_str(), widget.cVar,
                                                     std::get<UIWidgets::ComboMap_t>(options->comboVariant), *options);
                }
                if (result) {
                    if (widget.callback != nullptr) {
                        widget.callback(widget);
                    }
                }
            } break;
            case WIDGET_SLIDER_INT: {
                int32_t* pointer = std::get<int32_t*>(widget.valuePointer);
                if (pointer == nullptr) {
                    SPDLOG_ERROR("int32 Slider Widget requires a value pointer, currently nullptr");
                    assert(false);
                    return;
                }
                auto options = std::static_pointer_cast<UIWidgets::IntSliderOptions>(widget.options);
                options->color = menuThemeIndex;
                if (UIWidgets::SliderInt(widget.name.c_str(), pointer, *options)) {
                    if (widget.callback != nullptr) {
                        widget.callback(widget);
                    }
                };
            } break;
            case WIDGET_CVAR_SLIDER_INT: {
                auto options = std::static_pointer_cast<UIWidgets::IntSliderOptions>(widget.options);
                options->color = menuThemeIndex;
                if (UIWidgets::CVarSliderInt(widget.name.c_str(), widget.cVar, *options)) {
                    if (widget.callback != nullptr) {
                        widget.callback(widget);
                    }
                };
            } break;
            case WIDGET_SLIDER_FLOAT: {
                float* pointer = std::get<float*>(widget.valuePointer);

                if (pointer == nullptr) {
                    SPDLOG_ERROR("float Slider Widget requires a value pointer, currently nullptr");
                    assert(false);
                    return;
                }
                auto options = std::static_pointer_cast<UIWidgets::FloatSliderOptions>(widget.options);
                options->color = menuThemeIndex;
                if (UIWidgets::SliderFloat(widget.name.c_str(), pointer, *options)) {
                    if (widget.callback != nullptr) {
                        widget.callback(widget);
                    }
                }
            } break;
            case WIDGET_CVAR_SLIDER_FLOAT: {
                auto options = std::static_pointer_cast<UIWidgets::FloatSliderOptions>(widget.options);
                options->color = menuThemeIndex;
                if (UIWidgets::CVarSliderFloat(widget.name.c_str(), widget.cVar, *options)) {
                    if (widget.callback != nullptr) {
                        widget.callback(widget);
                    }
                }
            } break;
            case WIDGET_CVAR_BTN_SELECTOR: {
                auto options = std::static_pointer_cast<UIWidgets::BtnSelectorOptions>(widget.options);
                options->color = menuThemeIndex;
                if (UIWidgets::CVarBtnSelector(widget.name.c_str(), widget.cVar, *options)) {
                    if (widget.callback != nullptr) {
                        widget.callback(widget);
                    }
                }
            } break;
            case WIDGET_BUTTON: {
                auto options = std::static_pointer_cast<UIWidgets::ButtonOptions>(widget.options);
                options->color = menuThemeIndex;
                if (UIWidgets::Button(widget.name.c_str(), *options)) {
                    if (widget.callback != nullptr) {
                        widget.callback(widget);
                    }
                }
            } break;
            case WIDGET_CUSTOM: {
                if (widget.customFunction != nullptr) {
                    widget.customFunction(widget);
                }
            } break;
            case WIDGET_WINDOW_BUTTON: {
                if (widget.windowName == nullptr || widget.windowName[0] == '\0') {
                    std::string msg =
                        fmt::format("Error drawing window contents for {}: windowName not defined", widget.name);
                    SPDLOG_ERROR(msg.c_str());
                    break;
                }
                auto window = Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow(widget.windowName);
                if (!window) {
                    std::string msg =
                        fmt::format("Error drawing window contents: windowName {} does not exist", widget.windowName);
                    SPDLOG_ERROR(msg.c_str());
                    break;
                }
                auto options = std::static_pointer_cast<UIWidgets::WindowButtonOptions>(widget.options);
                options->color = menuThemeIndex;
                UIWidgets::WindowButton(widget.name.c_str(), widget.cVar, window, *options);
                if (!window->IsVisible()) {
#if defined(__ANDROID__)
                    // The integrated Mod Menu may use ordinary ImGui::Text(),
                    // which otherwise clips long descriptions at the right edge.
                    // Scope wrapping to this window to preserve all other menus.
                    const bool wrapModMenuText = std::string(widget.windowName) == "Mod Menu";
                    if (wrapModMenuText) {
                        ImGui::PushTextWrapPos(0.0f);
                    }
#endif
                    window->DrawElement();
#if defined(__ANDROID__)
                    if (wrapModMenuText) {
                        ImGui::PopTextWrapPos();
                    }
#endif
                }
            } break;
            case WIDGET_SEARCH: {
                UIWidgets::PushStyleButton(menuThemeIndex);
                if (ImGui::Button("Clear")) {
                    menuSearch.Clear();
                }
                ImGui::SameLine();
                if (CVarGetInteger("gSettings.Menu.SearchAutofocus", 0) &&
                    ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() &&
                    !ImGui::IsMouseClicked(0)) {
                    ImGui::SetKeyboardFocusHere(0);
                }
                UIWidgets::PushStyleCombobox(menuThemeIndex);
                ImGui::PushStyleColor(ImGuiCol_Border, UIWidgets::ColorValues.at(menuThemeIndex));
                menuSearch.Draw();
                ImGui::PopStyleColor();
                UIWidgets::PopStyleCombobox();
                UIWidgets::PopStyleButton();
                std::string menuSearchText(menuSearch.InputBuf);

                if (menuSearchText == "") {
                    ImGui::Text("Start typing to see results.");
                    return;
                }
                DrawSearchResults(menuSearchText);
#if !defined(__ANDROID__)
                ImGui::EndChild(); // Search Results
#endif
            } break;
            default:
                break;
        }
        if (widget.postFunc != nullptr) {
            widget.postFunc(widget);
        }
    } catch (const std::bad_variant_access& e) {
        SPDLOG_ERROR("Failed to draw menu item \"{}\" due to: {}", widget.name, e.what());
        assert(false);
    }
}

void Menu::Draw() {
    if (!IsVisible()) {
#if defined(__ANDROID__)
        menuSearch.Clear();
#endif
        return;
    }
    DrawElement();
    // Sync up the IsVisible flag if it was changed by ImGui
    SyncVisibilityConsoleVariable();
}

static bool freshOpen = true;
void Menu::DrawElement() {
    if (OTRGlobals::Instance->fontStandardLargest == nullptr) {
        return;
    }
    if (gGameState == nullptr) {
        return;
    }
    for (auto& [reason, info] : disabledMap) {
        info.active = info.evaluation(info);
    }
    auto menuThemeIndex = static_cast<UIWidgets::Colors>(CVarGetInteger("gSettings.Menu.Theme", defaultThemeIndex));

    windowHeight = ImGui::GetMainViewport()->WorkSize.y;
    windowWidth = ImGui::GetMainViewport()->WorkSize.x;
    auto windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
#if defined(__ANDROID__)
    const bool popout = false; // Mobile menu stays fullscreen; no detached menu window.
#else
    bool popout = CVarGetInteger("gSettings.Menu.Popout", 0) && allowPopout;
#endif
    if (popout) {
        windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking;
    }
    if (popout != popped) {
        if (popout) {
            windowHeight = poppedSize.y;
            windowWidth = poppedSize.x;
            ImGui::SetNextWindowSize({ static_cast<float>(windowWidth), static_cast<float>(windowHeight) },
                                     ImGuiCond_Always);
            ImGui::SetNextWindowPos(poppedPos, ImGuiCond_Always);
        } else if (popped) {
            CVarSetFloat("gSettings.Menu.PoppedWidth", poppedSize.x);
            CVarSetFloat("gSettings.Menu.PoppedHeight", poppedSize.y);
            CVarSave();
        }
    }
    popped = popout;
    auto windowCond = ImGuiCond_Always;
    if (!popout) {
        ImGui::SetNextWindowSize({ static_cast<float>(windowWidth), static_cast<float>(windowHeight) }, windowCond);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), windowCond, { 0.5f, 0.5f });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    }
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, CVarGetFloat("gSettings.Menu.BackgroundOpacity", 0.85f)));
    if (!ImGui::Begin("Main Menu", NULL, windowFlags)) {
        if (!popout) {
            ImGui::PopStyleVar();
        }
        freshOpen = true;
        ImGui::PopStyleColor();
        ImGui::End();
        return;
    }
    ImGui::PopStyleColor();
    if (popped != popout) {
        if (!popout) {
            ImGui::PopStyleVar();
        }
        CVarSetInteger("gSettings.Menu.Popout", popped);
        CVarSetFloat("gSettings.Menu.PoppedWidth", poppedSize.x);
        CVarSetFloat("gSettings.Menu.PoppedHeight", poppedSize.y);
        CVarSetFloat("gSettings.Menu.PoppedPos.x", poppedSize.x);
        CVarSetFloat("gSettings.Menu.PoppedPos.y", poppedSize.y);
        CVarSave();
        ImGui::End();
        return;
    }
#if !defined(__ANDROID__)
    ImGui::PushFont(OTRGlobals::Instance->fontStandardLargest);
#else
    ImGui::PushFont(OTRGlobals::Instance->fontStandardLarger);
#endif
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiStyle& style = ImGui::GetStyle();
    windowHeight = window->WorkRect.GetHeight();
    windowWidth = window->WorkRect.GetWidth();

#if defined(__ANDROID__)
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
#else
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 8.0f));
#endif
    const char* headerCvar = "gSettings.Menu.ActiveHeader";
    std::string headerIndex = CVarGetString(headerCvar, "Settings");
    if (GetVectorIndexOf(menuOrder, headerIndex) == menuOrder.size()) {
        headerIndex = menuOrder.at(0);
    }
    ImVec2 pos = window->DC.CursorPos;
    float centerX = pos.x + windowWidth / 2 - (style.ItemSpacing.x * (menuEntries.size() + 1));
    std::vector<ImVec2> headerSizes;
    bool autoFocus = CVarGetInteger("gSettings.Menu.SearchAutofocus", 0);
    bool headerSearch = !CVarGetInteger("gSettings.Menu.SidebarSearch", 0);
    float buttonWidth = (ImGui::CalcTextSize(ICON_FA_TIMES_CIRCLE) + style.FramePadding * 2).x;
    float reservedHeaderButtonWidth = buttonWidth * 3 + style.ItemSpacing.x * 3;
    float headerSearchWidth = 0.0f;
    float headerWidth = style.ItemSpacing.x;
#if !defined(__ANDROID__)
    if (headerSearch) {
        headerSearchWidth = 200.0f;
        headerWidth += headerSearchWidth + style.ItemSpacing.x + style.FramePadding.x;
    }
#endif
    for (auto& label : menuOrder) {
        ImVec2 size = ImGui::CalcTextSize(label.c_str());
        headerSizes.push_back(size);
        headerWidth += size.x + style.FramePadding.x * 2;
        if (label == headerIndex) {
            headerWidth += style.ItemSpacing.x;
        }
    }
    ImVec2 menuSize = { windowWidth, windowHeight };
#ifdef __ANDROID__
    if (windowWidth > 1280) {
        menuSize.x = std::fminf(windowWidth * 0.9f, windowHeight * 1.77f);
    }
    if (windowHeight > 800) {
        menuSize.y = windowHeight * 0.9f;
    }
#else
    menuSize = { std::fminf(1280, windowWidth), std::fminf(800, windowHeight) };
#endif
    pos += window->WorkRect.GetSize() / 2 - menuSize / 2;
    ImGui::SetNextWindowPos(pos);
    ImGui::BeginChild("Menu Block", menuSize, ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);

    std::unordered_map<std::string, SidebarEntry>* sidebar = &menuEntries.at(headerIndex).sidebars;
    float headerHeight = headerSizes.at(0).y + style.FramePadding.y * 2;
    float headerRowHeight = headerHeight;
#if defined(__ANDROID__)
    // The three action buttons have their own (larger) widget padding. Reserve
    // their real space instead of estimating it from the compact header style.
    const float touchScale = ImGui::GetIO().FontGlobalScale;
    // Keep an extra finger-sized gap between Search and the three critical
    // actions. Reserve the same gap here and when positioning the action buttons.
    const float androidHeaderActionGap = std::max(style.ItemSpacing.x, ImGui::GetFontSize() * 0.8f);
    const float actionsWidth = ImGui::CalcTextSize(ICON_FA_POWER_OFF).x +
                               ImGui::CalcTextSize(ICON_FA_UNDO).x +
                               ImGui::CalcTextSize(ICON_FA_TIMES_CIRCLE).x +
                               6.0f * 10.0f * touchScale + 4.0f * style.ItemSpacing.x +
                               androidHeaderActionGap;
    headerRowHeight = std::max(headerRowHeight, ImGui::GetFontSize() + 16.0f * touchScale);
    headerHeight = headerRowHeight;
    const float headerAvailableWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - actionsWidth);
#else
    ImVec2 buttonSize = ImGui::CalcTextSize(ICON_FA_TIMES_CIRCLE) + style.FramePadding * 2;
    ImGui::SameLine();
    float headerAvailableWidth = std::max(0.0f, menuSize.x - buttonSize.x * 3 - style.ItemSpacing.x * 3);
#endif
    ImVec2 headerSelSize = { headerAvailableWidth, headerRowHeight };
    ImGui::BeginChild("Header Selection", headerSelSize, ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
    std::string menuSearchText = "";
#if defined(__ANDROID__)
    // Android shows a real input field at all times. One touch activates it and
    // opens the soft keyboard; there is no intermediate search button.
    // All widths use the current ImGui font/style, so zoom is handled naturally.
    const float rowWidth = ImGui::GetContentRegionAvail().x;
    const float gap = style.ItemSpacing.x;
    float tabsWidth = 0.0f;
    for (size_t i = 0; i < menuOrder.size(); ++i) {
        tabsWidth += ImGui::CalcTextSize(menuEntries.at(menuOrder[i]).label.c_str()).x +
                     style.FramePadding.x * 2.0f;
        if (i != 0) {
            tabsWidth += gap;
        }
    }

    // Reserve a useful search width first. If the full tab bar won't fit next
    // to it, replace the tabs with a selector instead of adding a second row.
    const float desiredSearchWidth = headerSearch
        ? std::min(std::max(rowWidth * 0.32f, ImGui::GetFontSize() * 9.0f), rowWidth * 0.50f)
        : 0.0f;
    const bool showTabs = tabsWidth + (headerSearch ? gap + desiredSearchWidth : 0.0f) <= rowWidth;
    float searchWidth = desiredSearchWidth;

    if (showTabs) {
        size_t i = 0;
        for (const auto& label : menuOrder) {
            if (i++ != 0) {
                ImGui::SameLine();
            }
            auto& entry = menuEntries.at(label);
            const bool selected = headerIndex == label;
            UIWidgets::PushStyleButton(menuThemeIndex);
            if (!selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, { 0, 0, 0, 0 });
            }
            const bool clicked = ModernMenuHeaderEntry(entry.label);
            if (!selected) {
                ImGui::PopStyleColor();
            }
            UIWidgets::PopStyleButton();
            if (clicked && !selected) {
                headerIndex = label;
                CVarSetString(headerCvar, label.c_str());
                CVarSave();
                menuSearch.Clear();
            }
        }
    } else {
        // At higher zoom, keep navigation available without sacrificing Search.
        // Switch to an arrow-only combo if even the active tab title won't fit.
        const float minimumSearchWidth = headerSearch
            ? std::min(rowWidth * 0.72f, std::max(ImGui::GetFontSize() * 7.0f, ImGui::GetFrameHeight() * 3.0f))
            : 0.0f;
        const float availableComboWidth = std::max(1.0f, rowWidth -
            (headerSearch ? minimumSearchWidth + gap : 0.0f));
        const float labelledComboWidth = ImGui::CalcTextSize(menuEntries.at(headerIndex).label.c_str()).x +
                                        style.FramePadding.x * 2.0f + ImGui::GetFrameHeight();
        const bool compactCombo = availableComboWidth < labelledComboWidth;
        const float comboWidth = std::min(availableComboWidth,
            compactCombo ? ImGui::GetFrameHeight() : labelledComboWidth);
        ImGui::SetNextItemWidth(comboWidth);
        UIWidgets::PushStyleCombobox(menuThemeIndex);
        if (ImGui::BeginCombo("##AndroidHeaderTab", compactCombo ? "" : menuEntries.at(headerIndex).label.c_str(),
                              compactCombo ? ImGuiComboFlags_NoPreview : ImGuiComboFlags_None)) {
            for (const auto& label : menuOrder) {
                const bool selected = headerIndex == label;
                if (ImGui::Selectable(menuEntries.at(label).label.c_str(), selected)) {
                    headerIndex = label;
                    CVarSetString(headerCvar, label.c_str());
                    CVarSave();
                    menuSearch.Clear();
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        UIWidgets::PopStyleCombobox();
        searchWidth = headerSearch ? std::max(1.0f, rowWidth - comboWidth - gap) : 0.0f;
    }

    if (headerSearch) {
        // Keep Search immediately after the tabs (or compact selector).
        // Leave the remaining space between Search and the critical actions.
        ImGui::SameLine(0.0f, gap);
        const ImVec2 inputPos = ImGui::GetCursorScreenPos();
        auto color = UIWidgets::ColorValues.at(menuThemeIndex);
        color.w = 0.6f;
        ImGui::PushStyleColor(ImGuiCol_FrameBg, color);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        // Make the input as tall as the neighboring action buttons, not a tiny
        // text field with a small touch target.
        const float inputPadY = std::max(style.FramePadding.y,
            (headerRowHeight - ImGui::GetFontSize()) * 0.5f - 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, inputPadY));
        // Defer the Android request by one ImGui frame, so InputText has
        // already acquired focus before the native keyboard is opened.
        static int searchKeyboardRequestFrame = -1;
        menuSearch.Draw("##search", searchWidth);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            searchKeyboardRequestFrame = ImGui::GetFrameCount() + 1;
        }
        if (searchKeyboardRequestFrame >= 0 && ImGui::GetFrameCount() >= searchKeyboardRequestFrame) {
            if (ImGui::IsItemActive()) {
                AndroidRequestSearchKeyboard();
            }
            searchKeyboardRequestFrame = -1;
        }
        menuSearchText = menuSearch.InputBuf;
        menuSearchText.erase(std::remove(menuSearchText.begin(), menuSearchText.end(), ' '), menuSearchText.end());
        if (menuSearch.InputBuf[0] == '\0' && !ImGui::IsItemActive()) {
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(inputPos.x + style.FramePadding.x, inputPos.y + inputPadY),
                ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.4f)), "Search...");
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }
#else
    uint8_t curIndex = 0;
    for (auto& label : menuOrder) {
        if (curIndex != 0) {
            ImGui::SameLine();
        }
        auto& entry = menuEntries.at(label);
        std::string nextIndex = label;
        UIWidgets::PushStyleButton(menuThemeIndex);
        if (headerIndex != label) {
            ImGui::PushStyleColor(ImGuiCol_Button, { 0, 0, 0, 0 });
        }
        if (ModernMenuHeaderEntry(entry.label)) {
            if (headerSearch) {
                menuSearch.Clear();
            }
            CVarSetString(headerCvar, label.c_str());
            CVarSave();
            nextIndex = label;
        }
        if (headerIndex != label) {
            ImGui::PopStyleColor();
        }
        UIWidgets::PopStyleButton();
        if (headerIndex == label) {
            sidebar = &entry.sidebars;
        }
        if (nextIndex != label) {
            headerIndex = nextIndex;
        }
        curIndex++;
    }
    if (headerSearch) {
        ImGui::SameLine();
        if (autoFocus && freshOpen) {
            ImGui::SetKeyboardFocusHere();
        }
        auto color = UIWidgets::ColorValues.at(menuThemeIndex);
        color.w = 0.6f;
        ImGui::PushStyleColor(ImGuiCol_FrameBg, color);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        float searchWidth = std::min(headerSearchWidth, std::max(48.0f, ImGui::GetContentRegionAvail().x));
        menuSearch.Draw("##search", searchWidth);
        menuSearchText = menuSearch.InputBuf;
        menuSearchText.erase(std::remove(menuSearchText.begin(), menuSearchText.end(), ' '), menuSearchText.end());
        if (menuSearchText.length() < 1) {
            ImGui::SameLine(std::min(headerWidth, headerAvailableWidth) - searchWidth + style.ItemSpacing.x);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f), "Search...");
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
#endif
    // A compact selector or search can change the active header this frame.
    sidebar = &menuEntries.at(headerIndex).sidebars;
    ImGui::EndChild();
#if defined(__ANDROID__)
    ImGui::SameLine(0.0f, style.ItemSpacing.x + androidHeaderActionGap);
#else
    ImGui::SameLine(menuSize.x - (buttonSize.x * 3) - style.ItemSpacing.x * 3);
#endif
    UIWidgets::ButtonOptions options = {};
    options.color = UIWidgets::Colors::Red;
    options.size = UIWidgets::Sizes::Inline;
    options.tooltip = "Quit 2S2H";
    if (UIWidgets::Button(ICON_FA_POWER_OFF, options)) {
        BenGui::mModalWindow->RegisterPopup(
            "Quit 2S2H", "Are you sure you want to quit 2S2H?", "Quit", "Cancel",
            []() {
                std::shared_ptr<Menu> menu =
                    static_pointer_cast<Menu>(Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetMenu());
                if (!menu->IsMenuPopped()) {
                    menu->ToggleVisibility();
                }
                Ship::Context::GetRawInstance()->GetWindow()->Close();
            },
            nullptr);
    }
    ImGui::PopStyleVar();
    ImGui::SameLine();
    UIWidgets::ButtonOptions options2 = {};
    options2.color = UIWidgets::Colors::Red;
    options2.size = UIWidgets::Sizes::Inline;
    options2.tooltip = "Reset"
#ifdef __APPLE__
                       " (Command-R)"
#elif !defined(__SWITCH__) && !defined(__WIIU__)
                       " (Ctrl+R)"
#else
                       ""
#endif
        ;
    if (UIWidgets::Button(ICON_FA_UNDO, options2)) {
        std::reinterpret_pointer_cast<Ship::ConsoleWindow>(
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetGuiWindow("Console"))
            ->Dispatch("reset");
    }
    ImGui::SameLine();
    UIWidgets::ButtonOptions options3 = {};
    options3.size = UIWidgets::Sizes::Inline;
    options3.tooltip = "Close Menu (Esc)";
    if (UIWidgets::Button(ICON_FA_TIMES_CIRCLE, options3)) {
#if defined(__ANDROID__)
        menuSearch.Clear();
#endif
        ToggleVisibility();

        // Update gamepad navigation after close based on if other menus are still visible
        auto mImGuiIo = &ImGui::GetIO();
        if (CVarGetInteger(CVAR_IMGUI_CONTROLLER_NAV, 0) &&
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->GetMenuOrMenubarVisible()) {
            mImGuiIo->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        } else {
            mImGuiIo->ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
        }
    }

    pos.y += headerHeight + style.ItemSpacing.y;
    pos.x = centerX - menuSize.x / 2 + (style.ItemSpacing.x * (menuEntries.size() + 1));
    window->DrawList->AddRectFilled(pos, pos + ImVec2{ menuSize.x, 4 }, ImGui::GetColorU32({ 255, 255, 255, 255 }),
                                    true, style.WindowRounding);
    pos.y += style.ItemSpacing.y;
    float sectionHeight = menuSize.y - headerHeight - 4 - style.ItemSpacing.y * 2;
    float columnHeight = sectionHeight - style.ItemSpacing.y * 4;
    ImGui::SetNextWindowPos(pos + style.ItemSpacing * 2);
    float sidebarWidth = std::max(120.0f, menuSize.x * 0.14f) - style.ItemSpacing.x;

    const char* sidebarCvar = menuEntries.at(headerIndex).sidebarCvar;

    std::string sectionIndex = CVarGetString(sidebarCvar, "");
    if (GetVectorIndexOf(menuEntries[headerIndex].sidebarOrder, sectionIndex) ==
        menuEntries[headerIndex].sidebarOrder.size()) {
        sectionIndex = menuEntries[headerIndex].sidebarOrder.at(0);
    }

    float widestSidebarLabel = 0.0f;
    for (auto& sidebarLabel : menuEntries.at(headerIndex).sidebarOrder) {
        widestSidebarLabel = std::max(widestSidebarLabel, ImGui::CalcTextSize(sidebarLabel.c_str()).x);
    }
    sidebarWidth = std::max(sidebarWidth, widestSidebarLabel + style.FramePadding.x * 2 + style.ItemSpacing.x * 2);
#if defined(__ANDROID__)
    sidebarWidth = std::clamp(sidebarWidth, std::min(120.0f, menuSize.x * 0.24f), menuSize.x * 0.24f);
#else
    sidebarWidth = std::min(sidebarWidth, menuSize.x * 0.25f);
#endif

    float sectionCenterX = pos.x + (sidebarWidth / 2);
    float topY = pos.y;
    ImGui::BeginChild((menuEntries.at(headerIndex).label + " Section").c_str(), { sidebarWidth, columnHeight },
                      ImGuiChildFlags_None, ImGuiWindowFlags_NoTitleBar);
    for (auto& sidebarLabel : menuEntries.at(headerIndex).sidebarOrder) {
        std::string nextIndex = "";
        UIWidgets::PushStyleButton(menuThemeIndex);
        if (sectionIndex != sidebarLabel) {
            ImGui::PushStyleColor(ImGuiCol_Button, { 0, 0, 0, 0 });
        }
        if (ModernMenuSidebarEntry(sidebarLabel)) {
            if (headerSearch) {
                menuSearch.Clear();
            }
            CVarSetString(sidebarCvar, sidebarLabel.c_str());
            CVarSave();
            nextIndex = sidebarLabel;
        }
        if (sectionIndex != sidebarLabel) {
            ImGui::PopStyleColor();
        }
        UIWidgets::PopStyleButton();
        if (nextIndex != "") {
            sectionIndex = nextIndex;
        }
    }
    ImGui::EndChild();

#if defined(__ANDROID__)
    ImGui::PushFont(OTRGlobals::Instance->fontMonoLarger);
#else
    ImGui::PushFont(OTRGlobals::Instance->fontMonoLarger);
#endif
    pos = ImVec2{ sectionCenterX + (sidebarWidth / 2), topY } + style.ItemSpacing * 2;
    window->DrawList->AddRectFilled(pos, pos + ImVec2{ 4, sectionHeight - style.FramePadding.y * 2 },
                                    ImGui::GetColorU32({ 255, 255, 255, 255 }), true, style.WindowRounding);
    pos.x += 4 + style.ItemSpacing.x;
    ImGui::SetNextWindowPos(pos + style.ItemSpacing);
    float sectionWidth = menuSize.x - sidebarWidth - 4 - style.ItemSpacing.x * 4;
    std::string sectionMenuId = sectionIndex + " Settings";
    int columns = sidebar->at(sectionIndex).columnCount;
    size_t columnFuncs = sidebar->at(sectionIndex).columnWidgets.size();
    if (columnFuncs > 0) {
        columns = std::min<int>(columns, static_cast<int>(columnFuncs));
    }
    if (windowWidth < 800) {
        columns = 1;
    }
#ifdef __ANDROID__
    columns = 1;
#endif
    if (columns < 1) {
        columns = 1;
    }
    float columnWidth = (sectionWidth - style.ItemSpacing.x * columns) / columns;
    bool useColumns = columns > 1;
    if (!useColumns || (headerSearch && menuSearchText.length() > 0)) {
        ImGui::SameLine();
        ImGui::BeginChild(sectionMenuId.c_str(), { sectionWidth, columnHeight }, ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoTitleBar);
    }
    if (headerSearch && menuSearchText.length() > 0) {
        uint32_t searchCount = DrawSearchResults(menuSearchText);
        if (searchCount == 0) {
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("No results found").x) / 2);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f), "No results found");
        }
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Clear Search").x) / 2 - 10.0f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f);
        UIWidgets::ButtonOptions clearBtnOpts = {};
        clearBtnOpts.size = UIWidgets::Sizes::Inline;
        if (UIWidgets::Button("Clear Search", clearBtnOpts)) {
            menuSearch.Clear();
        }

#if !defined(__ANDROID__)
        ImGui::EndChild(); // Search Results
#endif
    } else {
        std::string menuLabel = menuEntries.at(headerIndex).label;
        if (MenuInit::GetUpdateFuncs().contains(menuLabel)) {
            if (MenuInit::GetUpdateFuncs()[menuLabel].contains(sectionIndex)) {
                for (auto& updateFunc : MenuInit::GetUpdateFuncs()[menuLabel][sectionIndex]) {
                    updateFunc();
                }
            }
        }
        for (size_t i = 0; i < columnFuncs; i++) {
            std::string sectionId = fmt::format("{} Column {}", sectionMenuId, i);
            if (useColumns) {
                ImGui::BeginChild(sectionId.c_str(), { columnWidth, columnHeight }, ImGuiChildFlags_None,
                                  ImGuiWindowFlags_NoTitleBar);
            }
            // for (auto& entryName : sidebar->at(sectionIndex).sidebarOrder) {
            for (auto& entry : sidebar->at(sectionIndex).columnWidgets.at(i)) {
                MenuDrawItem(entry, 90 / sidebar->at(sectionIndex).columnCount, menuThemeIndex);
            }
            //}
            if (useColumns) {
                ImGui::EndChild();
            }
            if (i < columns - 1) {
                ImGui::SameLine();
            }
        }
    }
    if (!useColumns || menuSearchText.length() > 0) {
        ImGui::EndChild();
    }
    ImGui::PopFont();
    ImGui::PopFont();

    if (!popout) {
        ImGui::PopStyleVar();
    }
    ImGui::EndChild();
    if (popout) {
        poppedSize = ImGui::GetWindowSize();
        poppedPos = ImGui::GetWindowPos();
    }
    if (freshOpen) {
        freshOpen = false;
    }
#if defined(__ANDROID__)
    AndroidMenuScrollWindowTree(window);
#endif
    ImGui::End();
}
} // namespace Ship
