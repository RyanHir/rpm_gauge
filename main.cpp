#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <map>
#include <string>
#include <string_view>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <vector>

#include "shader.hpp"
#include "text_renderer.hpp"

static constexpr int RPM_MIN = 0;
static constexpr int RPM_MAX = 3500;
static constexpr int RPM_STEP = 100;

static constexpr int NOTCHES = 8;
static constexpr float NOTCH_MAX = 0.90;
static constexpr float NOTCH_MAX_SMALL = 0.80;
static constexpr float NOTCH_MIN = 0.70;
static constexpr float NOTCH_WIDTH = 0.002;

static constexpr float NEEDLE_RANGE = 270;

static constexpr float DIAMETER = 0.75;
static constexpr float NEEDLE_WIDTH = 0.015;
static constexpr float NEEDLE_LENGTH = 0.6;
static constexpr float NEEDLE_OFFSET = 0.01;

struct datapack {
  glm::vec3 pos;
  glm::vec3 color;
};

struct vao_vbo {
  GLuint vao;
  GLuint vbo;
};

static std::vector<datapack> genCompleteBase();
static std::vector<datapack> genNeedle();
static int genShapeRenderingProgram();
static int genVao(std::vector<datapack> const &data);

template <typename T>
constexpr T map(T x, T x_low, T x_high, T t_low, T t_high) {
  return (x - x_low) * (t_high - t_low) / (x_high - x_low) + t_low;
}

int main() {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1); // macOS supports up to 4.1
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  GLFWwindow *window = glfwCreateWindow(1280, 720, "gauge", nullptr, nullptr);
  glfwMakeContextCurrent(window);
  int version = gladLoadGL(glfwGetProcAddress);
  glfwFocusWindow(window);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 410 core");

  std::vector<datapack> base = genCompleteBase();
  std::vector<datapack> needle = genNeedle();

  Program program = genShapeRenderingProgram();

  GLuint vao_base = genVao(base);
  GLuint vao_needle = genVao(needle);

  text_renderer text_renderer;
  text_renderer.allocate();
  text_renderer.set_color({0.3, 0.3, 0.3});

  glClearColor(0.2, 0.2, 0.2, 1.0);
  float angle{0}, hours{0};
  bool wireframe{false};
  float notch_text_scale = 1.0f;

  while (!glfwWindowShouldClose(window)) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    text_renderer.set_window_size(width, height);

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::SliderFloat("RPM", &angle, RPM_MIN, RPM_MAX);
    ImGui::InputFloat("Hours", &hours, 0.1, 1, "%0.1f");
    ImGui::Checkbox("Wireframe", &wireframe);
    glClear(GL_COLOR_BUFFER_BIT);

    if (wireframe) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glm::mat4 view = glm::identity<glm::mat4>();
    float scale = 0;
    if (width > height) {
      scale = float(height) / float(width);
      view = glm::scale(view, {scale, 1, 1});
    } else {
      scale = float(width) / float(height);
      view = glm::scale(view, {1, scale, 1});
    }

#if 1
    program.use();
    program.setUniform("view", view);
    program.setUniform("model", glm::identity<glm::mat4>());

    glBindVertexArray(vao_base);
    glDrawArrays(GL_TRIANGLES, 0, base.size());

    const float working_angle = map<float>(
        angle, RPM_MIN, RPM_MAX, -NEEDLE_RANGE / 2.0f, NEEDLE_RANGE / 2.0f);
    ImGui::Text("Working Angle: %0.3f", working_angle);
    glm::mat4 model =
        glm::rotate(glm::identity<glm::mat4>(), glm::radians(working_angle),
                    glm::vec3(0.0f, 0.0f, -1.0f));
    program.setUniform("model", model);

    glBindVertexArray(vao_needle);
    glDrawArrays(GL_TRIANGLES, 0, needle.size());
#endif

    ImGui::SliderFloat("Notch Text Scale", &notch_text_scale, 0.5, 1.5);
    for (int i = 0; i <= RPM_MAX / (RPM_STEP * 5); i++) {
      constexpr float min = 90.0f + (NEEDLE_RANGE / 2.0f);
      constexpr float max = 90.0f - (NEEDLE_RANGE / 2.0f);
      const float angle1 = glm::radians(map<float>(
          i, 0, static_cast<float>(RPM_MAX) / (RPM_STEP * 5), min, max));
      glm::vec2 pos{std::cos(angle1), std::sin(angle1)};
      pos = view * glm::vec4(pos * DIAMETER * NOTCH_MAX, 0, 0);
      pos = map<glm::vec2>(pos, {-1, -1}, {1, 1}, {0, 0}, {width, height});

      std::string label = std::to_string(i * RPM_STEP * 5 / 100);
      text_renderer.draw(label, pos.x, pos.y, notch_text_scale * scale);
    }

    glm::vec2 pos =
        map<glm::vec2>({0, -0.2}, {-1, -1}, {1, 1}, {0, 0}, {width, height});

    char hours_msg[32];
    std::snprintf(hours_msg, sizeof(hours_msg), "Hours %0.1f", hours);
    text_renderer.draw(hours_msg, pos.x, pos.y, notch_text_scale * scale);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwPollEvents();
    glfwSwapBuffers(window);
  }

  return 0;
}

static glm::vec3 Hue(float H) {
  H = map<float>(std::fmodf(H, 360), 0, 360, 0, 1);
  float R = std::abs(H * 6 - 3) - 1;
  float G = 2 - std::abs(H * 6 - 2);
  float B = 2 - std::abs(H * 6 - 4);
  return glm::clamp(glm::vec3(R, G, B));
}

static std::vector<datapack> genCompleteBase() {
  std::vector<datapack> ret;

  const float min = 90.0f + (NEEDLE_RANGE / 2.0f);
  const float max = 90.0f - (NEEDLE_RANGE / 2.0f);
  const int STEPS = RPM_MAX / RPM_STEP;

  const float HUE_RED = 0;
  const float HUE_YELLOW = 60;
  const float HUE_GREEN = 140;

  constexpr int CIRCLE_DIVISIONS = 64;
  // build circle
  for (int i = 0; i < CIRCLE_DIVISIONS; i++) {
    float o1 = (static_cast<float>(i) / CIRCLE_DIVISIONS) * (2.0f * M_PI);
    float o2 = (static_cast<float>(i + 1) / CIRCLE_DIVISIONS) * (2.0f * M_PI);
    ret.emplace_back(glm::vec3{0, 0, 0}, glm::vec3{0.7, 0.7, 0.7});
    ret.emplace_back(glm::vec3{std::cos(o1), std::sin(o1), 0},
                     glm::vec3{0.7, 0.7, 0.7});
    ret.emplace_back(glm::vec3{std::cos(o2), std::sin(o2), 0},
                     glm::vec3{0.7, 0.7, 0.7});
  }

  for (int rpm = RPM_MIN; rpm < RPM_MAX; rpm += RPM_STEP) {
    const float angle1 = glm::radians(map<float>(rpm, 0, RPM_MAX, min, max));
    const float angle2 =
        glm::radians(map<float>(rpm + RPM_STEP, 0, RPM_MAX, min, max));
    const float c1 = std::cos(angle1);
    const float s1 = std::sin(angle1);
    const float c2 = std::cos(angle2);
    const float s2 = std::sin(angle2);

    glm::vec3 color;
    if (rpm < 500 || (rpm >= 2600 && rpm < 2800)) {
      color = Hue(HUE_YELLOW);
    } else if (rpm >= 2800) {
      color = Hue(HUE_RED);
    } else {
      color = Hue(HUE_GREEN);
    }

    ret.emplace_back(glm::vec3{c1 * NOTCH_MAX_SMALL, s1 * NOTCH_MAX_SMALL, 1},
                     color);
    ret.emplace_back(glm::vec3{c1 * NOTCH_MIN, s1 * NOTCH_MIN, 1}, color);
    ret.emplace_back(glm::vec3{c2 * NOTCH_MIN, s2 * NOTCH_MIN, 1}, color);

    ret.emplace_back(glm::vec3{c1 * NOTCH_MAX_SMALL, s1 * NOTCH_MAX_SMALL, 1},
                     color);
    ret.emplace_back(glm::vec3{c2 * NOTCH_MAX_SMALL, s2 * NOTCH_MAX_SMALL, 1},
                     color);
    ret.emplace_back(glm::vec3{c2 * NOTCH_MIN, s2 * NOTCH_MIN, 1}, color);
  }

  const datapack notch_small[] = {
      // triangle 1
      {glm::vec3{NOTCH_MIN, +NOTCH_WIDTH / 2, 3}, glm::vec3{0.2, 0.2, 0.2}},
      {glm::vec3{NOTCH_MIN, -NOTCH_WIDTH / 2, 3}, glm::vec3{0.2, 0.2, 0.2}},
      {glm::vec3{NOTCH_MAX_SMALL, +NOTCH_WIDTH / 2, 3},
       glm::vec3{0.2, 0.2, 0.2}},
      // triangle 2
      {glm::vec3{NOTCH_MIN, -NOTCH_WIDTH / 2, 3}, glm::vec3{0.2, 0.2, 0.2}},
      {glm::vec3{NOTCH_MAX_SMALL, +NOTCH_WIDTH / 2, 3},
       glm::vec3{0.2, 0.2, 0.2}},
      {glm::vec3{NOTCH_MAX_SMALL, -NOTCH_WIDTH / 2, 3},
       glm::vec3{0.2, 0.2, 0.2}},
  };
  const datapack notch_large[] = {
      // triangle 1
      {glm::vec3{NOTCH_MIN, +NOTCH_WIDTH / 2, 3}, glm::vec3{0.2, 0.2, 0.2}},
      {glm::vec3{NOTCH_MIN, -NOTCH_WIDTH / 2, 3}, glm::vec3{0.2, 0.2, 0.2}},
      {glm::vec3{NOTCH_MAX, +NOTCH_WIDTH / 2, 3}, glm::vec3{0.2, 0.2, 0.2}},
      // triangle 2
      {glm::vec3{NOTCH_MIN, -NOTCH_WIDTH / 2, 3}, glm::vec3{0.2, 0.2, 0.2}},
      {glm::vec3{NOTCH_MAX, +NOTCH_WIDTH / 2, 3}, glm::vec3{0.2, 0.2, 0.2}},
      {glm::vec3{NOTCH_MAX, -NOTCH_WIDTH / 2, 3}, glm::vec3{0.2, 0.2, 0.2}},
  };

  for (int rpm = RPM_MIN; rpm <= RPM_MAX; rpm += RPM_STEP) {
    const float i_ = static_cast<float>(rpm) / (NOTCHES - 1);
    const float angle =
        glm::radians(map<float>(rpm, RPM_MIN, RPM_MAX, min, max));

    glm::mat4 mat = glm::rotate(glm::identity<glm::mat4>(), angle, {0, 0, 1});
    const datapack *cbegin, *cend;
    if (rpm % (RPM_STEP * 5) == 0) {
      cbegin = std::cbegin(notch_large);
      cend = std::cend(notch_large);
    } else {
      cbegin = std::cbegin(notch_small);
      cend = std::cend(notch_small);
    }
    std::transform(cbegin, cend, std::back_inserter(ret), [&](datapack data) {
      data.pos = mat * glm::vec4(data.pos, 0);
      return data;
    });
  }

  for (datapack &vert : ret) {
    vert.pos *= glm::vec3{DIAMETER, DIAMETER, 1.0};
  }

  return ret;
}

static std::vector<datapack> genNeedle() {
  std::vector<datapack> ret = {
      // only four verticies are needed since we are using GL_TRIANGLE_STRIP
      // base
      {glm::vec3{-NEEDLE_WIDTH / 2, -NEEDLE_OFFSET, 0}, {0.6, 0.4, 0.4}},
      {glm::vec3{+NEEDLE_WIDTH / 2, -NEEDLE_OFFSET, 0}, {0.6, 0.4, 0.4}},
      // edge
      {glm::vec3{-NEEDLE_WIDTH / 2, NEEDLE_LENGTH - NEEDLE_OFFSET, 0},
       {0.6, 0.2, 0.2}},

      {glm::vec3{+NEEDLE_WIDTH / 2, -NEEDLE_OFFSET, 0}, {0.6, 0.4, 0.4}},
      {glm::vec3{-NEEDLE_WIDTH / 2, NEEDLE_LENGTH - NEEDLE_OFFSET, 0},
       {0.6, 0.2, 0.2}},

      {glm::vec3{+NEEDLE_WIDTH / 2, NEEDLE_LENGTH - NEEDLE_OFFSET, 0},
       {0.6, 0.2, 0.2}},
  };

  return ret;
}

static int compileProgram(const char *vert, const char *frag) {
  GLint success = 0;

  GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vert_shader, 1, &vert, NULL);
  glCompileShader(vert_shader);
  glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &success);
  assert(success);

  GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(frag_shader, 1, &frag, NULL);
  glCompileShader(frag_shader);
  glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &success);
  assert(success);

  GLuint program = glCreateProgram();

  glAttachShader(program, vert_shader);
  glAttachShader(program, frag_shader);
  glLinkProgram(program);
  glDetachShader(program, vert_shader);
  glDetachShader(program, frag_shader);

  glDeleteShader(vert_shader);
  glDeleteShader(frag_shader);

  glGetProgramiv(program, GL_LINK_STATUS, (int *)&success);
  assert(success);

  return program;
}

static int genShapeRenderingProgram() {
  static const char *vert = R"GLSL(#version 410 core
  layout(location=0) in vec2 position;
  layout(location=1) in vec3 color;
  
  uniform mat4 model;
  uniform mat4 view;
  
  out vec3 Color;
  
  void main()
  {
    gl_Position = (view * model) * vec4(position, 0.0, 1.0);
    Color = color;
  }
  )GLSL";

  static const char *frag = R"GLSL(#version 410 core
  in vec3 Color;
  out vec4 outColor;
  
  void main()
  {
    outColor = vec4(Color, 1.0);
  }
  )GLSL";
  return compileProgram(vert, frag);
}

static int genVao(std::vector<datapack> const &data) {
  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(datapack), data.data(),
               GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(datapack),
                        (void *)offsetof(datapack, pos));
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(datapack),
                        (void *)offsetof(datapack, color));
  return vao;
}
