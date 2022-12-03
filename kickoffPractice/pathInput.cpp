#include "pch.h"
#include "pathInput.h"


namespace fs = std::filesystem;

int InputPath::compteur = 0;

std::string InputPath::main()
{
    static float scroll_to_off_px = 0.0f;
    static float scroll_to_pos_px = 200.0f;

    ImGui::Indent(100);


    ImGuiStyle& style = ImGui::GetStyle();

    ImGui::PushID(compteur);
    ImGui::BeginGroup();
   
    ImGui::TextUnformatted("");

    const ImGuiWindowFlags child_flags = 0;
    const ImGuiID child_id = ImGui::GetID((void*)(intptr_t)0);
    const bool child_is_visible = ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true, child_flags);
    

    if (child_is_visible) // Avoid calling SetScrollHereY when running with culled items
    {
        if (currentFolder != "C:\\")
        {
            if (ImGui::Selectable("..",false, ImGuiSelectableFlags_AllowDoubleClick))
            {
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    currentFolder = currentFolder.parent_path();
                }
            }
        }
        try
        {
            ImGui::Indent(5);
            for (const auto& entry : fs::directory_iterator(currentFolder))
            {
                if (entry.is_directory())
                {
                    auto currentPath = entry.path();
                    if (ImGui::Selectable(currentPath.filename().string().c_str(), currentPath.string() == selectedPath, ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        if (ImGui::IsMouseDoubleClicked(0))
                        {
                            currentFolder = currentPath;
                        }
                        else
                        {
                            selectedPath = currentPath.string();
                        }
                    }
                }
            }
        }
        catch (std::filesystem::filesystem_error const& ex)
        {
            LOG(ex.code().message());
            currentFolder = currentFolder.parent_path();
        }
    }
    float scroll_y = ImGui::GetScrollY();
    float scroll_max_y = ImGui::GetScrollMaxY();
    ImGui::EndChild();
    ImGui::EndGroup();    
    ImGui::PopID();
    if (ImGui::Button("Select"))
    {
        return selectedPath;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Select the highlighted folder or current folder if none is highlighted");
    }
	return "";
}

InputPath::InputPath()
{
    compteur++;
    currentFolder = "C:\\";
    selectedPath = "";
}
