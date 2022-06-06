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

	Vertex vertices[6] =
	{
		{{1.0f,-1.0f,0.0f},{1.0f,1.0f} },
		{{1.0f,1.0f,0.0f}, {1.0f,0.0f}},
		{{-1.0f,1.0f,0.0f}, {0.0f,0.0f}},
		{{-1.0f,1.0f,0.0f}, {0.0f,0.0f}},
		{{-1.0f,-1.0f,0.0f}, {0.0f,1.0f} },
		{{ 1.0f,-1.0f,0.0f}, {1.0f,1.0f} }
	};

	glGenBuffers(1, &m_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
	glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

	glGenVertexArrays(1, &m_VAO);
	glBindVertexArray(m_VAO);

	glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	GLuint m_vs = 0, m_fs = 0;

	std::string m_vsSource =
#include"Shaders/vertex.h"
		;

	std::string m_fsSource =
#include"Shaders/fragment.h"
		;

	const char* m_vsSourcePtr = m_vsSource.c_str();	//account for opengl 2-level pointer bs
	const char* m_fsSourcePtr = m_fsSource.c_str();

	m_vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(m_vs, 1, &m_vsSourcePtr, 0);
	glCompileShader(m_vs);

	m_fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(m_fs, 1, &m_fsSourcePtr, 0);
	glCompileShader(m_fs);

	char buf[512];
	glGetShaderInfoLog(m_fs, 512, nullptr, buf);
	std::cout << buf;

	m_program = glCreateProgram();
	glAttachShader(m_program, m_vs);
	glAttachShader(m_program, m_fs);
	glLinkProgram(m_program);
	glUseProgram(m_program);


	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);	//override driver's default 4-byte unpack stride, maybe reudndant
	glGenTextures(1, &m_texHandle);
	glBindTexture(GL_TEXTURE_2D, m_texHandle);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 240, 160, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glfwSwapInterval(1);


	GuiRenderer::init(m_window);
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
	GuiRenderer::prepareFrame();
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(m_program);
	glBindVertexArray(m_VAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_texHandle);
	
	glDrawArrays(GL_TRIANGLES, 0, 6);
	GuiRenderer::render();

	glfwSwapBuffers(m_window);
}

void Display::update(void* data)
{
	glBindTexture(GL_TEXTURE_2D, m_texHandle);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 240, 160, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, data);
}

bool Display::getPressed(unsigned int key)
{
	return (glfwGetKey(m_window, key)) == GLFW_PRESS;
}