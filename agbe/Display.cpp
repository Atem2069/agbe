#include"Display.h"

Display::Display(int scaleFactor)
{
	Logger::getInstance()->msg(LoggerSeverity::Info, std::format("Init display - width={0} height={1}", 240 * scaleFactor, 160 * scaleFactor));
	if (!glfwInit())
		return;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	m_window = glfwCreateWindow(240 * scaleFactor, 160 * scaleFactor, "LDMIA", nullptr, nullptr);
	if (!m_window)
	{
		Logger::getInstance()->msg(LoggerSeverity::Error, "Failed to init OpenGL context!!!");
		return;
	}

	glfwMakeContextCurrent(m_window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		Logger::getInstance()->msg(LoggerSeverity::Error, "Failed to init GLAD!!!");
		return;
	}
}

Display::~Display()
{
	glfwDestroyWindow(m_window);
	glfwTerminate();
}

bool Display::getShouldClose()
{
	return glfwWindowShouldClose(m_window);
}

void Display::draw()
{
	glfwPollEvents();
	glClear(GL_COLOR_BUFFER_BIT);

	//gl calls

	glfwSwapBuffers(m_window);
}

void Display::update(void* data)
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
}