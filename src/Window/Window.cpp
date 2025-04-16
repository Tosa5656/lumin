#include <Window/Window.h>

Window::Window(std::string title, int width, int height, int x, int y)
{
    m_Window = nullptr;

	m_Title = title;
	m_Width = width;
	m_Height = height;
	m_X = x;
	m_Y = y;

    Init();
}

void Window::Init()
{
    if (!glfwInit())
        return;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_Window = glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), NULL, NULL);
    glfwSetWindowPos(m_Window, m_X, m_Y);

    if (!m_Window)
    {
        glfwTerminate();
        return;
    }

    glfwMakeContextCurrent(m_Window);

    int version = gladLoadGL();
    if (version == 0) {
        std::cout << "Failed to initialize OpenGL context" << std::endl;
        return;
    }

    SetBackgroundColor(Color(0.2f, 0.3f, 0.7f, 1.0f));

    Shader vertexShader = Shader("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
    Shader fragmentShader = Shader("shaders/fragment_shader.glsl", GL_FRAGMENT_SHADER);
    
    ShaderProgram shaderProgram = ShaderProgram(vertexShader, fragmentShader);
    shaderProgram.LinkShaders();

    glDeleteShader(vertexShader.getId());
    glDeleteShader(fragmentShader.getId());

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };

    float colors[] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    };
    
    unsigned int VBO[2], VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(2, VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    while (!glfwWindowShouldClose(m_Window))
    {
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram.getId());
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        glfwSwapBuffers(m_Window);

        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(2, VBO);
    glDeleteProgram(shaderProgram.getId());

    glfwTerminate();
}