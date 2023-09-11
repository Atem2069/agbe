#include"GuiRenderer.h"

void GuiRenderer::init(GLFWwindow* window)
{
	ImGui::CreateContext();
	ImGui::StyleColorsClassic();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();
}

void GuiRenderer::prepareFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void GuiRenderer::render()
{
	ImVec2 cursorPos = ImGui::GetIO().MousePos;
	bool showMenuOverride = (cursorPos.x >= 0 && cursorPos.y >= 0) && (cursorPos.y <= 25);
	if ((m_autoHideMenu && showMenuOverride) || !m_autoHideMenu || m_menuItemSelected)
	{
		if (ImGui::BeginMainMenuBar())
		{
			m_menuItemSelected = false;
			if (ImGui::BeginMenu("File"))
			{
				m_menuItemSelected = true;
				ImGui::MenuItem("Open...", nullptr, &m_openFileDialog);
				ImGui::MenuItem("Exit", nullptr, nullptr);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Debug"))
			{
				m_menuItemSelected = true;
				ImGui::MenuItem("Disable vid sync", nullptr, &Config::GBA.disableVideoSync);
				ImGui::EndMenu();
			}
		}
		ImGui::EndMainMenuBar();
	}

	if (m_openFileDialog)
	{
		OPENFILENAMEA ofn = {};
		CHAR szFile[255] = { 0 };

		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = NULL;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = "Game Boy Advance ROM Files\0*.gba\0";
		ofn.nFilterIndex = 1;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = NULL;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

		if (GetOpenFileNameA(&ofn) == TRUE)
		{
			std::string filename = szFile;
			Config::GBA.RomName = filename;
			Config::GBA.shouldReset = true;
			//Config::GB.System.RomName = filename;
			//Config::GB.System.reset = true;
		}

		m_openFileDialog = false;
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool GuiRenderer::m_openFileDialog = false;
bool GuiRenderer::m_autoHideMenu = true;
bool GuiRenderer::m_menuItemSelected = false;