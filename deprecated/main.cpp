/* Grua (texturas + iluminacion). Practica COGA 2025    *|
|* ---------------------------------------------------- *|
|* Autores:                                             *|
|*      Hugo Coto Florez                                *|
|*      Guillermo Fernandez                             *|
|*                                                      *|
|* License: licenseless                                 *|
|*                                                      *|
|* Mensaje del autor: No me hago responsable            *|
|* de la perdida de cordura por parte del lector        *|
|* ni actividades de escasa legitimidad que             *|
|* puedan ocasionarse total o parcialmente tras         *|
|* haber intentado entender este codigo. Un abrazo.     *|
|*                                                      *|
|* ---------------------------------------------------- */

/* || USER OPTIONS || */ /* Enable VSync. FPS limited to screen refresh rate */
/* || USER OPTIONS || */ /*(0: disable, 1: enable, undef: default) */
/* || USER OPTIONS || */ #define VSYNC 0
/* || USER OPTIONS || */
/* || USER OPTIONS || */ /* Show fps if SHOW_FPS is defined and not 0 */
/* || USER OPTIONS || */ #define SHOW_FPS 0

/* Todo: refactorizar a little :)
 * (>1k lines is not right, right?)
 * (1200 after embed shaders) */

#if defined(_WIN32)
#include "include/glad/glad.h"

#include "include/glfw/include/GLFW/glfw3.h"
#include "include/glm/ext.hpp"
#include "include/glm/glm.hpp"

#elif defined(linux)
#include <glad/glad.h>

#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <glm/ext.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/glm.hpp>

#else
static_assert(0 == "Undefined target");
#endif

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

// #include "thirdparty/frog/frog.h"

#include "setShaders.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// im lazy
using namespace std;
using namespace glm;

// Return a RGB normalized coma separeted equivalent color
#define HexColor(hex_color)                     \
        ((hex_color & 0xFF0000) >> 16) / 256.0, \
        ((hex_color & 0xFF00) >> 8) / 256.0,    \
        ((hex_color & 0xFF)) / 256.0

// #define BG_COLOR HexColor(0x87CEEB), 1.0
#define BG_COLOR HexColor(0x000000), 1.0

#define PI 3.1416f
#define PIMED 1.5708f

#define Point(a, b, c) a, b, c
#define Face4(a, b, c, d) a, b, c, a, c, d
#define vec_ptr(vec, i) ((vec).data() + (i))
#define SIZE(arr) (sizeof((arr)) / sizeof(*(arr)))
#define Texture(a, b) a, b
#define Normal(a, b, c) a, b, c

#define $(a) #a "\n"

struct gvopts {
        GLuint vertex_start, vertex_coords;
        GLuint texture_start, texture_coords;
        GLuint normal_start, normal_coords;
        GLuint padd;
        bool use_vertex, use_texture, use_normal;
};

typedef struct Material {
        const char *name;
        float Ns; // Brillo especular
        float Ka[3]; // Color de la luz ambiental
        float Ks[3]; // Specular
        float Kd[3]; // Diffuse Reflectivity
        float Ke[3]; // Color de emision
        float Ni; // indice de refraccion
        float d; // transparencia
        int illum; // modelo de iluminacion
        unsigned int texture;
        unsigned char *image;
        int height;
        int width;
        int comp;
        unsigned int id;
} Material;

typedef struct Object {
        const char *name;
        GLuint vao;
        GLuint indexes_n;
        GLuint printable;
        int color;
        GLuint shader_program;
        mat4 model; // relative model
        mat4 default_model; // default starting relative model
        vector<Object *> attached;
        Object *parent;
        Material material;
        void (*before_draw)(struct Object &self);
} Object;

Material default_mat = (Material) {
        .name = "Default",
        .Ns = 200.0f, // Brillo especular
        .Ka = { 0.2f, 0.2f, 0.2f }, // Luz ambiental
        .Ks = { 0.4f, 0.4f, 0.4f }, // Reflectividad especular
        .Kd = { 20.8f, 20.8f, 20.8f }, // Color difuso
        .Ke = { 0.0f, 0.0f, 0.0f }, // Emisión (ninguna)
        .Ni = 1.0f, // Índice de refracción
        .d = 1.0f, // Opacidad
        .illum = 0,
        .texture = 0,
        .image = 0,
        .height = 0,
        .width = 0,
        .comp = 0,
        .id = 0,
};

vector<Object> objects;

GLuint WIDTH = 640;
GLuint HEIGHT = 480;

vec3 lightPos = vec3(0.0f, 0.0f, 0.0f); // Posición de la luz en el mundo
vec3 lightColor = vec3(1.0f, 1.0f, 1.0f); // Blanco
float lightIntensity = 10.0f;

unsigned int c_lock = 0;
float camera_pos_y = 48.0f;
float camera_pos_x = 24.0f;

/* As vel = vel_base / prev_fps,
 * if prev_fps is 0 in the first second
 * movement in infinity. Changing this variable
 * to max value (unsigned -1 )block movement until
 * prev_fps is calulated */
float interframe_time = 0;

enum {
        VIEW_3_PERSON,
        VIEW_1_PERSON,
        VIEW_NOFOLLOW,
} VIEW = VIEW_NOFOLLOW;

typedef enum {
        OBJ_GROUND = 0,
        OBJ_BASE,
        OBJ_HEAD,
        OBJ_WHEEL_FL,
        OBJ_WHEEL_FR,
        OBJ_WHEEL_BL,
        OBJ_WHEEL_BR,
        OBJ_SPHERE_BASE,
        OBJ_SPHERE,
        OBJ_PALO,
        OBJ_A,
        OBJ_LIGHT_SPOT1,
        OBJ_LIGHT_SPOT2,
        OBJ_TREE1,
        OBJ_TREE2,
        OBJ_TREE3,
        OBJ_TREE4,
        OBJ_TREE5,
        OBJ_TREE6,
        OBJ_TREE7,
        OBJ_TREE8,
        OBJ_TREE9,
        OBJ_TREE10,
} ObjectsId;

static GLuint
use_global_shader(GLuint shader = 0)
{
        static GLuint global_shader = 0;
        if (shader) {
                global_shader = shader;
        }

        return global_shader;
}

static inline vec3
get_model_position(mat4 model)
{
        return vec3(model[3]);
}

static mat4
get_obj_absolute_model(Object obj)
{
        if (obj.parent) {
                return get_obj_absolute_model(*obj.parent) * obj.model;
        } else {
                return obj.model;
        }
}

static inline vec3
get_obj_absolute_position(Object obj)
{
        return get_model_position(get_obj_absolute_model(obj));
}

static inline mat3
get_model_rotation_matrix(mat4 model)
{
        return mat3(model);
}

static inline vec3
get_model_rotation(mat4 model)
{
        return eulerAngles(quat_cast(mat3(model)));
}

// Add a new objetc to global obj array
static void
new_object(const char *name, void (*get_vao)(GLuint *, GLuint *),
           int color = 0xFFFFFF, // white by default
           bool printable = 1) // printable by default
{
        GLuint vao, indexes_n;
        get_vao(&vao, &indexes_n);

        objects.push_back(
        (Object) {
        .name = name,
        .vao = vao,
        .indexes_n = indexes_n,
        .printable = printable,
        .color = color,
        .shader_program = use_global_shader(0),
        .model = mat4(1.0f),
        .default_model = mat4(1.0f),
        .attached = vector<Object *>(),
        .parent = NULL,
        .material = default_mat,
        .before_draw = NULL,
        });
}

static inline void
show_help()
{
        printf("[W]: Moverse hacia delante\n");
        printf("[A]: Girar a la izquierda\n");
        printf("[S]: Moverse hacia atras\n");
        printf("[D]: Girar a la derecha\n");
        printf("[H]: Mover el cuerpo a la izquierda\n");
        printf("[J]: Mover la grua hacia abajo\n");
        printf("[K]: Mover la grua hacia arriba\n");
        printf("[L]: Mover el cuerpo a la derecha\n");
        printf("[C]: Cambiar camara\n");
        printf("[up]: Subir la camara\n");
        printf("[left]: Acercar la camara\n");
        printf("[down]: Bajar la camara\n");
        printf("[right]: Alejar la camara\n");
}

GLuint
load_texture(Material &mat, int how = 0)
{
        if (mat.texture > 0) {
                // printf("Texture loaded yet\n");
                return mat.texture;
        }

        if (mat.image == NULL) {
                // printf("Material has no image data!\n");
                return 0;
        }
        glGenTextures(1, &mat.texture);
        glBindTexture(GL_TEXTURE_2D, mat.texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, how ?: GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, how ?: GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        assert(mat.width > 0 && mat.height > 0);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mat.width, mat.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, mat.image);
        glGenerateMipmap(GL_TEXTURE_2D);

        glBindTexture(GL_TEXTURE_2D, 0);

        assert(mat.texture > 0);
        return mat.texture;
}

void
add_material_image(const char *str, Material &material, int how = 0)
{
        stbi_set_flip_vertically_on_load(1);

        material.image = stbi_load(str, &material.width, &material.height, &material.comp, STBI_rgb_alpha);

        if (material.image == NULL) {
                fprintf(stderr, "Failed to load texture '%s'\n", str);
                exit(0);
        }
        load_texture(material, how);
        stbi_image_free(material.image);
}

static void
gen_vao(GLuint *VAO, size_t n, const float *vertex, size_t m, const GLuint *indexes,
        struct gvopts opts)
{
        GLuint VBO, EBO;
        glGenVertexArrays(1, VAO);
        glBindVertexArray(*VAO);
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, n * sizeof(*vertex), vertex, GL_STATIC_DRAW);
        glGenBuffers(1, &EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m * sizeof(*indexes), indexes, GL_STATIC_DRAW);

        if (opts.use_vertex) {
                glVertexAttribPointer(0, opts.vertex_coords, GL_FLOAT, GL_FALSE,
                                      opts.padd * sizeof(float), (void *) (sizeof(float) * opts.vertex_start));
                glEnableVertexAttribArray(0);
        }
        if (opts.use_texture) {
                glVertexAttribPointer(1, opts.texture_coords, GL_FLOAT, GL_FALSE,
                                      opts.padd * sizeof(float), (void *) (sizeof(float) * opts.texture_start));
                glEnableVertexAttribArray(1);
        }
        if (opts.use_normal) {
                glVertexAttribPointer(2, opts.normal_coords, GL_FLOAT, GL_FALSE,
                                      opts.padd * sizeof(float), (void *) (sizeof(float) * opts.normal_start));
                glEnableVertexAttribArray(2);
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        glDeleteBuffers(1, &EBO);
        glDeleteBuffers(1, &VBO);
}

void
generateSphere(float radius, unsigned int sectorCount, unsigned int stackCount,
               std::vector<float> &vertices, std::vector<unsigned int> &indices)
{
        vertices.clear();
        indices.clear();

        for (unsigned int i = 0; i <= stackCount; ++i) {
                float stackAngle = PI / 2 - i * (PI / stackCount);
                float xy = radius * cosf(stackAngle);
                float z = radius * sinf(stackAngle);

                for (unsigned int j = 0; j <= sectorCount; ++j) {
                        float sectorAngle = j * (2 * PI / sectorCount);
                        float x = xy * cosf(sectorAngle);
                        float y = xy * sinf(sectorAngle);

                        vertices.push_back(x);
                        vertices.push_back(y);
                        vertices.push_back(z);
                }
        }

        for (unsigned int i = 0; i < stackCount; ++i) {
                for (unsigned int j = 0; j < sectorCount; ++j) {
                        unsigned int first = i * (sectorCount + 1) + j;
                        unsigned int second = first + sectorCount + 1;

                        indices.push_back(first);
                        indices.push_back(second);
                        indices.push_back(first + 1);

                        indices.push_back(second);
                        indices.push_back(second + 1);
                        indices.push_back(first + 1);
                }
        }
}

void
sphere(GLuint *VAO, GLuint *indexes_n, float radius)
{
        std::vector<float> vertices;
        std::vector<unsigned int> indices;

        generateSphere(radius, 10, 10, vertices, indices);
        *indexes_n = indices.size();

        struct gvopts opts;
        opts.vertex_start = 0;
        opts.vertex_coords = 3;
        opts.padd = 3;
        opts.use_vertex = true;
        opts.use_texture = false;
        opts.use_normal = false;

        gen_vao(VAO, vertices.size(), vertices.data(), indices.size(), indices.data(), opts);
}

void
square(GLuint *VAO, GLuint *indexes_n, float x, float texture_scale = 1, float relation = 1)
{
        float vertices[] = {
                Point(-x / 2.0f, 0.0f, -x / 2.0f), Texture(0, 0),
                Point(-x / 2.0f, 0.0f, x / 2.0f), Texture(0, texture_scale * relation),
                Point(x / 2.0f, 0.0f, x / 2.0f), Texture(texture_scale * relation, texture_scale * relation),
                Point(x / 2.0f, 0.0f, -x / 2.0f), Texture(texture_scale * relation, 0),

                /* 3 ---- 2
                 * |      |
                 * |      |
                 * 0 ---- 1 */
        };

        unsigned int indices[] = {
                Face4(0, 1, 2, 3),
        };

        *indexes_n = SIZE(indices);

        struct gvopts opts;
        opts.vertex_start = 0;
        opts.vertex_coords = 3;
        opts.texture_start = 3;
        opts.texture_coords = 2;
        opts.padd = 5;
        opts.use_vertex = true;
        opts.use_texture = true;
        opts.use_normal = false;

        gen_vao(VAO, SIZE(vertices), vertices, SIZE(indices), indices, opts);
}

/* Create a cube with a given (x, y, z) size */
static void
cube_textured(GLuint *VAO, GLuint *indexes_n, float x, float y, float z, float texture_scale = 1, float relation = 1)
{
        /*                                       | y
     0(-x,y,-z)-> /---------/|<- (x,y,-z)4       |
                 / |       / |                   |______ x
                /  |      /  |                  /
  1(-x,y,z)->  /_________/   |  <- (x,y,z)5    / z
 2(-x,-y,-z)-> |   /-----|---/ <- (x,-y,-z)6
               |  /      |  /
               | /       | /
  3(-x,-y,z)-> |/________|/ <- (x,-y,z)7


        */

        x /= 2;
        y /= 2;
        z /= 2;

        float vertices[] = {
                Point(-x, y, -z), Texture(0, 0),
                Point(-x, y, z), Texture(texture_scale * relation, 0),
                Point(-x, -y, -z), Texture(texture_scale * relation, texture_scale * relation),
                Point(-x, -y, z), Texture(0, texture_scale * relation),
                Point(x, y, -z), Texture(0, texture_scale * relation),
                Point(x, y, z), Texture(texture_scale * relation, texture_scale * relation),
                Point(x, -y, -z), Texture(texture_scale * relation, 0),
                Point(x, -y, z), Texture(0, 0)
        };

        GLuint indices[] = {
                Face4(0, 1, 5, 4), // top
                Face4(0, 2, 3, 1), // left
                Face4(6, 4, 5, 7), // right
                Face4(1, 3, 7, 5), // front
                Face4(0, 4, 6, 2), // back
                Face4(3, 2, 6, 7), // down
        };

        *indexes_n = SIZE(indices);
        struct gvopts opts;
        opts.vertex_start = 0;
        opts.vertex_coords = 3;
        opts.texture_start = 3;
        opts.texture_coords = 2;
        opts.padd = 5;
        opts.use_vertex = true;
        opts.use_texture = true;
        opts.use_normal = false;

        gen_vao(VAO, SIZE(vertices), vertices, SIZE(indices), indices, opts);
}

/* Create a cube with a given (x, y, z) size */
static void
cube(GLuint *VAO, GLuint *indexes_n, float x, float y, float z)
{
        /*                                       | y
     0(-x,y,-z)-> /---------/|<- (x,y,-z)4       |
                 / |       / |                   |______ x
                /  |      /  |                  /
  1(-x,y,z)->  /_________/   |  <- (x,y,z)5    / z
 2(-x,-y,-z)-> |   /-----|---/ <- (x,-y,-z)6
               |  /      |  /
               | /       | /
  3(-x,-y,z)-> |/________|/ <- (x,-y,z)7

        */

        x /= 2;
        y /= 2;
        z /= 2;

        float vertices[] = {
                Point(-x, y, -z),
                Point(-x, y, z),
                Point(-x, -y, -z),
                Point(-x, -y, z),
                Point(x, y, -z),
                Point(x, y, z),
                Point(x, -y, -z),
                Point(x, -y, z),
        };

        GLuint indices[] = {
                Face4(0, 1, 5, 4), // top
                Face4(0, 2, 3, 1), // left
                Face4(6, 4, 5, 7), // right
                Face4(1, 3, 7, 5), // front
                Face4(0, 4, 6, 2), // back
                Face4(3, 2, 6, 7), // down
        };

        *indexes_n = SIZE(indices);

        struct gvopts opts;
        opts.vertex_start = 0;
        opts.vertex_coords = 3;
        opts.padd = 3;
        opts.use_vertex = true;
        opts.use_texture = false;
        opts.use_normal = false;

        gen_vao(VAO, SIZE(vertices), vertices, SIZE(indices), indices, opts);
}

static inline void
get_base_vao(GLuint *vao, GLuint *indexes_n)
{
        cube_textured(vao, indexes_n, 7, 1.5, 3, 1.0);
}

static inline void
get_head_vao(GLuint *vao, GLuint *indexes_n)
{
        cube_textured(vao, indexes_n, 2, 3, 3.2);
}

static inline void
get_wheel_vao(GLuint *vao, GLuint *indexes_n)
{
        cube(vao, indexes_n, 1, 1, 0.5);
}

void
get_circle_base_vao(GLuint *vao, GLuint *indexes_n)
{
        cube(vao, indexes_n, 1.0, 1.0, 1.0);
}

void
get_circle_vao(GLuint *vao, GLuint *indexes_n)
{
        sphere(vao, indexes_n, 0.5);
}

void
get_palo_vao(GLuint *vao, GLuint *indexes_n)
{
        cube(vao, indexes_n, 0.5, 5.0, 0.5);
}

static inline void
get_ground_vao(GLuint *VAO, GLuint *indexes_n)
{
        square(VAO, indexes_n, 1000, 80);
}

static inline void
get_A_vao(GLuint *VAO, GLuint *indexes_n)
{
        cube(VAO, indexes_n, 1, 6, 1);
}

static inline void
get_tree_vao(GLuint *VAO, GLuint *indexes_n)
{
        square(VAO, indexes_n, 6);
}

static inline void
get_lightspot_vao(GLuint *VAO, GLuint *indexes_n)
{
        cube(VAO, indexes_n, 1.5, 0.5, 0.5);
}

static void
process_input(GLFWwindow *window)
{
        static float moveSpeed = 0;
        float moveInc = 0.06f * (interframe_time);
        float moveDec = moveInc / 2;
        float cameraSpeed = 6.0f * (interframe_time);
        float rotateSpeed = 1.0f * (interframe_time);
        float maxspeed = 200 * moveInc;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window, true);

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                if (moveSpeed < maxspeed) {
                        moveSpeed += moveInc;
                        // printf("SpeedUp %f\n", moveSpeed);
                }
                // move obj
                objects.at(OBJ_BASE).model = translate(objects.at(OBJ_BASE).model, vec3(moveSpeed, 0, 0));
                // rotate wheel
                objects.at(OBJ_WHEEL_FL).model = rotate(objects.at(OBJ_WHEEL_FL).model, moveSpeed, vec3(0, 0, -1));
                objects.at(OBJ_WHEEL_FR).model = rotate(objects.at(OBJ_WHEEL_FR).model, moveSpeed, vec3(0, 0, -1));
                objects.at(OBJ_WHEEL_BL).model = rotate(objects.at(OBJ_WHEEL_BL).model, moveSpeed, vec3(0, 0, -1));
                objects.at(OBJ_WHEEL_BR).model = rotate(objects.at(OBJ_WHEEL_BR).model, moveSpeed, vec3(0, 0, -1));
        }

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_RELEASE) {
                moveSpeed -= moveDec;
                if ((moveSpeed) < 0) {
                        moveSpeed = 0;
                } else {
                        // printf("SpeedDown %f\n", moveSpeed);
                        //   move obj
                        objects.at(OBJ_BASE).model = translate(objects.at(OBJ_BASE).model, vec3(moveSpeed, 0, 0));
                        // rotate wheel
                        objects.at(OBJ_WHEEL_FL).model = rotate(objects.at(OBJ_WHEEL_FL).model, moveSpeed, vec3(0, 0, -1));
                        objects.at(OBJ_WHEEL_FR).model = rotate(objects.at(OBJ_WHEEL_FR).model, moveSpeed, vec3(0, 0, -1));
                        objects.at(OBJ_WHEEL_BL).model = rotate(objects.at(OBJ_WHEEL_BL).model, moveSpeed, vec3(0, 0, -1));
                        objects.at(OBJ_WHEEL_BR).model = rotate(objects.at(OBJ_WHEEL_BR).model, moveSpeed, vec3(0, 0, -1));
                }
        }

        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                // move obj
                if (moveSpeed > 0) {
                        moveSpeed -= moveDec;
                        if ((moveSpeed) < 0)
                                moveSpeed = 0;
                        else {
                                // printf("SpeedDown %f\n", moveSpeed);
                        }
                } else {
                        moveSpeed = 100 * moveInc;
                        // printf("SpeedUp Reverse %f\n", moveSpeed);
                        objects.at(OBJ_BASE).model = translate(objects.at(OBJ_BASE).model, vec3(-moveSpeed, 0, 0));
                        // rotate wheel
                        objects.at(OBJ_WHEEL_FL).model = rotate(objects.at(OBJ_WHEEL_FL).model, moveSpeed, vec3(0, 0, 1));
                        objects.at(OBJ_WHEEL_FR).model = rotate(objects.at(OBJ_WHEEL_FR).model, moveSpeed, vec3(0, 0, 1));
                        objects.at(OBJ_WHEEL_BL).model = rotate(objects.at(OBJ_WHEEL_BL).model, moveSpeed, vec3(0, 0, 1));
                        objects.at(OBJ_WHEEL_BR).model = rotate(objects.at(OBJ_WHEEL_BR).model, moveSpeed, vec3(0, 0, 1));
                        moveSpeed = 0;
                }
        }

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                objects.at(OBJ_BASE).model = rotate(objects.at(OBJ_BASE).model, rotateSpeed, vec3(0, 1, 0));
        }

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                objects.at(OBJ_BASE).model = rotate(objects.at(OBJ_BASE).model, rotateSpeed, vec3(0, -1, 0));
        }

        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) {
                if (get_model_rotation(objects.at(OBJ_SPHERE).model).z < PIMED)
                        objects.at(OBJ_SPHERE).model = rotate(objects.at(OBJ_SPHERE).model, rotateSpeed, vec3(0, 0, 1));
        }

        if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) {
                if (get_model_rotation(objects.at(OBJ_SPHERE).model).z > -PIMED)
                        objects.at(OBJ_SPHERE).model = rotate(objects.at(OBJ_SPHERE).model, rotateSpeed, vec3(0, 0, -1));
        }

        if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS) {
                objects.at(OBJ_SPHERE_BASE).model = rotate(objects.at(OBJ_SPHERE_BASE).model, rotateSpeed, vec3(0, 1, 0));
        }

        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
                objects.at(OBJ_SPHERE_BASE).model = rotate(objects.at(OBJ_SPHERE_BASE).model, rotateSpeed, vec3(0, -1, 0));
        }

        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && !c_lock) {
                if (VIEW == VIEW_3_PERSON)
                        VIEW = VIEW_NOFOLLOW;
                else if (VIEW == VIEW_NOFOLLOW)
                        VIEW = VIEW_1_PERSON;
                else
                        VIEW = VIEW_3_PERSON;
                c_lock = 1;
        }

        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE && c_lock)
                c_lock = 0;

        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
                camera_pos_y += cameraSpeed;
        }

        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
                camera_pos_y -= cameraSpeed;
        }

        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
                camera_pos_x += cameraSpeed;
        }

        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
                camera_pos_x -= cameraSpeed;
        }
}

static void
__framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
        WIDTH = width;
        HEIGHT = height;
        glViewport(0, 0, width, height);
}


vec3 cameraPosition;

void
__look_at_view(Object &object)
{
        vec3 obj_pos = get_model_position(object.model);
        mat3 rotation = get_model_rotation_matrix(object.default_model);
        vec3 view_pos = cameraPosition;
        vec3 to_cam = normalize(vec3(view_pos.x - obj_pos.x, 0.0, view_pos.z - obj_pos.z));

        // El ángulo que queremos aplicar sobre el eje Y global
        float angle = atan2(to_cam.x, to_cam.z); // atan2(X, Z) -> rotación Y para mirar

        mat4 model = mat4(1.0f);
        model = translate(model, obj_pos);
        model = rotate(model, angle, vec3(0.0, 1.0, 0.0));
        model *= mat4(rotation);
        object.model = model;
}

void
set_camera(Object &object)
{
        vec3 cameraEye;
        vec3 cameraUp;
        mat4 m;
        vec3 dirf;
        mat4 view;
        GLuint viewLoc = glGetUniformLocation(object.shader_program, "view");
        GLuint projectionLoc = glGetUniformLocation(object.shader_program, "projection");
        mat4 projection = perspective(radians(45.0f), (float) WIDTH / (float) HEIGHT, 0.1f, 100.0f);

        switch (VIEW) {
        case VIEW_NOFOLLOW:
                cameraEye = vec3(0, 0, 0);
                cameraPosition = vec3(camera_pos_x, camera_pos_y, 0);
                cameraUp = vec3(0.0f, 1.0f, 0.0f);
                break;

        case VIEW_3_PERSON:
                m = objects.at(OBJ_BASE).model;
                dirf = normalize(vec3(m[0]));
                cameraEye = get_obj_absolute_position(objects.at(OBJ_BASE));
                cameraPosition = cameraEye - dirf * camera_pos_x + vec3(0.0f, camera_pos_y, 0.0f);
                cameraUp = vec3(m[1]);
                break;

        case VIEW_1_PERSON:
                m = objects.at(OBJ_BASE).model * (objects.at(OBJ_HEAD).model);
                cameraPosition = get_model_position(m);
                dirf = normalize(vec3(m[0]));
                cameraEye = cameraPosition + dirf;
                cameraUp = vec3(objects.at(OBJ_BASE).model[1]);
                break;
        }

        view = lookAt(cameraPosition, cameraEye, cameraUp);

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, value_ptr(projection));
}


void
set_light(Object light1, Object light2)
{
        mat4 model = get_obj_absolute_model(light1);
        vec3 local_light_pos = vec3(0.0f, 0.0f, 0.0f); // Posición relativa dentro del objeto
        lightPos = vec3(model * vec4(local_light_pos, 1.0f)); // Aplicar la transformación completa
        vec3 dirf = normalize(vec3(model[0]));
        glUniform3fv(glGetUniformLocation(light1.shader_program, "lightPos1"), 1, value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(light1.shader_program, "lightDir1"), 1, value_ptr(dirf));

        model = get_obj_absolute_model(light2);
        local_light_pos = vec3(0.0f, 0.0f, 0.0f); // Posición relativa dentro del objeto
        lightPos = vec3(model * vec4(local_light_pos, 1.0f)); // Aplicar la transformación completa
        dirf = normalize(vec3(model[0]));
        glUniform3fv(glGetUniformLocation(light1.shader_program, "lightPos2"), 1, value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(light1.shader_program, "lightDir2"), 1, value_ptr(dirf));

        vec3 viewPos = cameraPosition;
        // printf("Light pos: %f, %f, %f\n", lightPos.x, lightPos.y, lightPos.z);
        glUniform3fv(glGetUniformLocation(light1.shader_program, "viewPos"), 1, value_ptr(viewPos));
        glUniform3fv(glGetUniformLocation(light1.shader_program, "lightColor"), 1, value_ptr(lightColor * lightIntensity));
        glUniform3fv(glGetUniformLocation(light1.shader_program, "Ka"), 1, light1.material.Ka);
        glUniform3fv(glGetUniformLocation(light1.shader_program, "Ks"), 1, light1.material.Ks);
        glUniform3fv(glGetUniformLocation(light1.shader_program, "Kd"), 1, light1.material.Kd);
        glUniform3fv(glGetUniformLocation(light1.shader_program, "Ke"), 1, light1.material.Ke);
        glUniform1f(glGetUniformLocation(light1.shader_program, "Ns"), light1.material.Ns);
        glUniform1f(glGetUniformLocation(light1.shader_program, "Ni"), light1.material.Ni);
        glUniform1f(glGetUniformLocation(light1.shader_program, "d"), light1.material.d);


        /* Not suported in windows? */
        // glShadeModel(GL_SMOOTH);
}


void
set_texture_n_color(Object &object)
{
        GLint colorLoc;
        GLint textureLoc;
        colorLoc = glGetUniformLocation(object.shader_program, "color");
        textureLoc = glGetUniformLocation(object.shader_program, "texture1");
        if (colorLoc != -1)
                glUniform3f(colorLoc, HexColor(object.color));

        glUniform1i(glGetUniformLocation(object.shader_program, "useTexture"), object.material.texture != 0);
        if (textureLoc != -1 && glIsTexture(object.material.texture)) {
                // printf("Using texture %d\n",object.material.texture);
                glEnable(GL_TEXTURE_2D);
                glActiveTexture(GL_TEXTURE0);
                glUniform1i(textureLoc, 0);
                glBindTexture(GL_TEXTURE_2D, object.material.texture);

        } else if (object.material.texture != 0) {
                // printf("Texture %d failed!\n", object.material.texture);
                // exit(object.material.texture);
        }
}


static void
draw_object(Object &object, mat4 model = mat4(1.0f))
{
        static GLuint active_shader = 0;
        GLuint modelLoc;

        // get obj model
        model = model * object.model;

        // draw attached objects
        for (Object *attached_obj : object.attached)
                draw_object(*attached_obj, model);

        if (object.printable == 0)
                return;

        if (object.before_draw)
                object.before_draw(object);

        if (active_shader != object.shader_program) {
                // printf("Shader set to %d\n", object.shader_program);
                glUseProgram(object.shader_program);
                active_shader = object.shader_program;
        }

        set_camera(object);
        set_texture_n_color(object);

        modelLoc = glGetUniformLocation(object.shader_program, "model");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(model));

        glBindVertexArray(object.vao);
        glDrawElements(GL_TRIANGLES, object.indexes_n, GL_UNSIGNED_INT, 0);

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
}

void
mouse_callback(GLFWwindow *window, double xposIn, double yposIn)
{
        static bool firstMouse = true;
        static float lastX;
        // static float lastY;
        float xpos = static_cast<float>(xposIn);
        // float ypos = static_cast<float>(yposIn);
        if (firstMouse) {
                lastX = xpos;
                // lastY = ypos;
                firstMouse = false;
        }

        float xoffset = xpos - lastX;
        // float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

        lastX = xpos;
        // lastY = ypos;

        float rotateSpeed = 0.01f;

        objects.at(OBJ_BASE).model = rotate(objects.at(OBJ_BASE).model, rotateSpeed * xoffset, vec3(0, -1, 0));
}

static void
attach_object(Object *parent, Object *child)
{
        if (child->parent)
                return;
        parent->attached.push_back(child);
        child->parent = parent;
}

static void
set_before_draw(Object *obj, void (*func)(Object &))
{
        obj->before_draw = func;
}

static void
fps()
{
        static time_t last_time = 0;
        static unsigned int fps = 0;
        time_t t;
        struct timespec tp;
        static struct timespec last_tp = { 0, 0 };

        if (clock_gettime(CLOCK_REALTIME, &tp) < 0)
                return;

        t = glfwGetTime();

        ++fps;

        interframe_time = tp.tv_sec - last_tp.tv_sec + (tp.tv_nsec - last_tp.tv_nsec) * 1e-9;
        // printf("interframe_time: %f\n", interframe_time);

        last_tp = tp;

        if (t - last_time >= 1) {
                last_time = t;
#if defined(SHOW_FPS) && SHOW_FPS
                printf("[FPS] %u\n", fps);
#endif
                fps = 0;
        }
}

/* Main loop. Executed until program is closed manually. */
int
mainloop(GLFWwindow *window)
{
        /* Execute until window is closed */
        while (!glfwWindowShouldClose(window)) {
                process_input(window);
                fps();

                glClearColor(BG_COLOR);
                glClear(GL_COLOR_BUFFER_BIT);
                glClear(GL_DEPTH_BUFFER_BIT);

                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                set_light(objects.at(OBJ_LIGHT_SPOT1), objects.at(OBJ_LIGHT_SPOT2));
                draw_object(objects.at(OBJ_GROUND));
                draw_object(objects.at(OBJ_A));
                draw_object(objects.at(OBJ_BASE));

                draw_object(objects.at(OBJ_TREE1));
                draw_object(objects.at(OBJ_TREE2));
                draw_object(objects.at(OBJ_TREE3));
                draw_object(objects.at(OBJ_TREE4));
                draw_object(objects.at(OBJ_TREE5));
                draw_object(objects.at(OBJ_TREE6));
                draw_object(objects.at(OBJ_TREE7));
                draw_object(objects.at(OBJ_TREE8));
                draw_object(objects.at(OBJ_TREE9));
                draw_object(objects.at(OBJ_TREE10));
                /* Every object attached to OBJ_BASE is automatically drawn */

                glfwSwapBuffers(window);
                glfwPollEvents();
        }

        for (struct Object &obj : objects) {
                glDeleteVertexArrays(1, &obj.vao);
        }

        return 0;
}

static const char vertex_shader_color[] =
$(#version 330 core)

$(layout(location = 0) in vec3 aPos;)
$(layout(location = 1) in vec2 aTexCoord;)
$(layout(location = 2) in vec3 aNormal;)

$(uniform mat4 model;)
$(uniform mat4 view;)
$(uniform mat4 projection;)
$(uniform vec3 lightDir1;)
$(uniform vec3 lightDir2;)
$(out vec2 TexCoord;)
$(out vec3 FragPos;)
$(out vec3 Normal;)
$(out vec3 lightDirection1;)
$(out vec3 lightDirection2;)

$(void main())
$({ )
    $(gl_Position = projection * view * model * vec4(aPos, 1.0);)
    $(FragPos = vec3(model * vec4(aPos, 1.0));)
    $(TexCoord = aTexCoord;)

    $(lightDirection1 = normalize(lightDir1);)
    $(lightDirection2 = normalize(lightDir2);)
    $( }) "\0";

static const char fragment_shader_color[] =
$(#version 330 core)

$(in vec3 FragPos;)
$(in vec2 TexCoord;)
$(in vec3 lightDirection1;)
$(in vec3 lightDirection2;)
$(out vec4 FragColor;)
$(uniform vec3 lightPos1;)
$(uniform vec3 lightPos2;)
$(uniform vec3 lightColor;)
$(uniform vec3 Ka;)
$(uniform vec3 Kd;)
$(uniform vec3 viewPos;)
$(uniform vec3 color;)
$(uniform sampler2D texture1;)
$(uniform int useTexture;)

$(void)
$(main())
$({ )

    $(vec3 dX = dFdx(FragPos);)
    $(vec3 dY = dFdy(FragPos);)
    $(vec3 norm = normalize(cross(dX, dY));)
    $(float ambientStrength = 0.4;)
    $(vec3 ambient = ambientStrength * lightColor;)
    $(vec3 light = ambient * Ka;)
    $(vec3 lightDirNorm1 = normalize(lightPos1 - FragPos);)
    $(vec3 lightDirNorm2 = normalize(lightPos2 - FragPos);)
    $(float distance1 = length(lightPos1 - FragPos);)
    $(float distance2 = length(lightPos2 - FragPos);)

    $(float Kc = 2.0;)
    $(float Kl = 1.2;)
    $(float Kq = 0.09;)

    $(float attenuation1 = 1.0 /(Kc + Kl * distance1 + Kq *(distance1 * distance1));)
    $(float attenuation2 = 1.0 /(Kc + Kl * distance2 + Kq *(distance2 * distance2));)
    $(float falloffFactor = 2.0;)
    $(float centerIntensity1 = pow(max(dot(norm, lightDirNorm1), 0.0), falloffFactor);)
    $(float centerIntensity2 = pow(max(dot(norm, lightDirNorm1), 0.0), falloffFactor);)
    $(vec3 fd1 = normalize(FragPos - lightPos1);)
    $(vec3 fd2 = normalize(FragPos - lightPos2);)

    $(if (acos(dot(fd1, lightDirection1))<radians(15.0)))
    $({ )
        $(float diff = max(dot(norm, lightDirNorm1), 0.0);)
        $(vec3 diffuse = diff * Kd * attenuation1 * lightColor;)
        $(light += diffuse;)
        $(float specularStrength = 0.5;)
        $(vec3 viewDir = normalize(viewPos - FragPos);)
        $(vec3 reflectDir = reflect(- lightDirNorm1, norm);)
        $(float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);)
        $(vec3 specular = specularStrength * spec * lightColor * attenuation1;)
        $(light += specular;)
        $( })

    $(else if (acos(dot(fd2, lightDirection2))<radians(15.0)))
    $({ )
        $(float diff = max(dot(norm, lightDirNorm2), 0.0);)
        $(vec3 diffuse = diff * Kd * attenuation2 * lightColor;)
        $(light += diffuse;)
        $(float specularStrength = 0.5;)
        $(vec3 viewDir = normalize(viewPos - FragPos);)
        $(vec3 reflectDir = reflect(- lightDirNorm2, norm);)
        $(float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);)
        $(vec3 specular = specularStrength * spec * lightColor * attenuation2;)
        $(light += specular;)
        $( })

    $(if (useTexture == 1))
    $({ )
        $(vec4 texColor = texture(texture1, TexCoord);)
        $(if (texColor.a<0.1))
        $({ )
            $(discard;)
            $( })
        $(FragColor = vec4(light * color, 1.0) * texColor;)
        $( })
    $(else)
    $({ )
        $(FragColor = vec4(light * color, 1.0);)
        $( })
    $( }) "\n\0";

void
init_object_vector(vector<Object> &objs)
{
        for (Object& o : objs) {
                o.model = o.default_model;
        }
}

void
init_objects()
{
        GLuint shader_program = setShaders_str(vertex_shader_color, fragment_shader_color);
        assert(shader_program > 0);

        use_global_shader(shader_program);

        new_object("ground", get_ground_vao);
        new_object("base", get_base_vao);
        new_object("head", get_head_vao);
        new_object("wheel_fl", get_wheel_vao, 0x444444);
        new_object("wheel_fr", get_wheel_vao, 0x444444);
        new_object("wheel_bl", get_wheel_vao, 0x444444);
        new_object("wheel_br", get_wheel_vao, 0x444444);
        new_object("circle_base", get_circle_base_vao);
        new_object("circle", get_circle_vao);
        new_object("palo", get_palo_vao);
        new_object("A", get_A_vao, 0xAACC00);
        new_object("Light Spot 1", get_lightspot_vao, 0xFFFFFF, false);
        new_object("Light Spot 2", get_lightspot_vao, 0xFFFFFF, false);
        new_object("tree1", get_tree_vao);
        new_object("tree2", get_tree_vao);
        new_object("tree3", get_tree_vao);
        new_object("tree4", get_tree_vao);
        new_object("tree5", get_tree_vao);
        new_object("tree6", get_tree_vao);
        new_object("tree7", get_tree_vao);
        new_object("tree8", get_tree_vao);
        new_object("tree9", get_tree_vao);
        new_object("tree10", get_tree_vao);

        add_material_image("./textures/StripedAsphalt/Striped_Asphalt_ufoidcskw_1K_BaseColor.jpg",
                           objects.at(OBJ_GROUND).material);

        add_material_image("./textures/bluePlastic/Scratched_Polypropylene_Plastic_schbehmp_1K_BaseColor.jpg",
                           objects.at(OBJ_PALO).material);

        add_material_image("./textures/marbleCheckeredFloor/Marble_Checkered_Floor_sescnen_1K_BaseColor.jpg",
                           objects.at(OBJ_BASE).material);

        add_material_image("./textures/tree2d.png",
                           objects.at(OBJ_TREE1).material, GL_CLAMP_TO_EDGE);

        objects.at(OBJ_HEAD).material = objects.at(OBJ_BASE).material;
        objects.at(OBJ_SPHERE_BASE).material = objects.at(OBJ_PALO).material;
        objects.at(OBJ_SPHERE).material = objects.at(OBJ_PALO).material;

        objects.at(OBJ_TREE2).material = objects.at(OBJ_TREE1).material;
        objects.at(OBJ_TREE3).material = objects.at(OBJ_TREE1).material;
        objects.at(OBJ_TREE4).material = objects.at(OBJ_TREE1).material;
        objects.at(OBJ_TREE5).material = objects.at(OBJ_TREE1).material;
        objects.at(OBJ_TREE6).material = objects.at(OBJ_TREE1).material;
        objects.at(OBJ_TREE7).material = objects.at(OBJ_TREE1).material;
        objects.at(OBJ_TREE8).material = objects.at(OBJ_TREE1).material;
        objects.at(OBJ_TREE9).material = objects.at(OBJ_TREE1).material;
        objects.at(OBJ_TREE10).material = objects.at(OBJ_TREE1).material;

        // A
        objects.at(OBJ_A).default_model = translate(objects.at(OBJ_A).default_model, vec3(6.0, 3, 0.0));

        // Base
        objects.at(OBJ_BASE).default_model = rotate(objects.at(OBJ_BASE).default_model, PIMED, vec3(0.0, 1.0, 0.0));
        objects.at(OBJ_BASE).default_model = translate(objects.at(OBJ_BASE).default_model, vec3(0.0, 1.39, 0.0));

        // Head
        attach_object(vec_ptr(objects, OBJ_BASE), vec_ptr(objects, OBJ_HEAD));
        objects.at(OBJ_HEAD).default_model = translate(objects.at(OBJ_HEAD).default_model, vec3(2.75, 2.0, 0.0));

        // wheel_fl
        attach_object(vec_ptr(objects, OBJ_BASE), vec_ptr(objects, OBJ_WHEEL_FL));
        objects.at(OBJ_WHEEL_FL).default_model = translate(objects.at(OBJ_WHEEL_FL).default_model, vec3(2.5, -0.75, -1.5));

        // wheel_fr
        attach_object(vec_ptr(objects, OBJ_BASE), vec_ptr(objects, OBJ_WHEEL_FR));
        objects.at(OBJ_WHEEL_FR).default_model = translate(objects.at(OBJ_WHEEL_FR).default_model, vec3(2.5, -0.75, +1.5));

        // wheel_bl
        attach_object(vec_ptr(objects, OBJ_BASE), vec_ptr(objects, OBJ_WHEEL_BL));
        objects.at(OBJ_WHEEL_BL).default_model = translate(objects.at(OBJ_WHEEL_BL).default_model, vec3(-2.5, -0.75, -1.5));

        // wheel_br
        attach_object(vec_ptr(objects, OBJ_BASE), vec_ptr(objects, OBJ_WHEEL_BR));
        objects.at(OBJ_WHEEL_BR).default_model = translate(objects.at(OBJ_WHEEL_BR).default_model, vec3(-2.5, -0.75, +1.5));

        // sphere base
        attach_object(vec_ptr(objects, OBJ_BASE), vec_ptr(objects, OBJ_SPHERE_BASE));
        objects.at(OBJ_SPHERE_BASE).default_model = translate(objects.at(OBJ_SPHERE_BASE).default_model, vec3(-1.0, 0.75, 0.0));

        // sphere
        attach_object(vec_ptr(objects, OBJ_SPHERE_BASE), vec_ptr(objects, OBJ_SPHERE));
        objects.at(OBJ_SPHERE).default_model = translate(objects.at(OBJ_SPHERE).default_model, vec3(0.0, 0.5, 0.0));

        // palo
        attach_object(vec_ptr(objects, OBJ_SPHERE), vec_ptr(objects, OBJ_PALO));
        objects.at(OBJ_PALO).default_model = translate(objects.at(OBJ_PALO).default_model, vec3(0.0, 2.75, 0.0));

        // light spot (where light is placed)
        attach_object(vec_ptr(objects, OBJ_HEAD), vec_ptr(objects, OBJ_LIGHT_SPOT1));
        objects.at(OBJ_LIGHT_SPOT1).default_model = translate(objects.at(OBJ_LIGHT_SPOT1).default_model, vec3(0.0, 0.0, -1.0));

        attach_object(vec_ptr(objects, OBJ_HEAD), vec_ptr(objects, OBJ_LIGHT_SPOT2));
        objects.at(OBJ_LIGHT_SPOT2).default_model = translate(objects.at(OBJ_LIGHT_SPOT2).default_model, vec3(0.0, 0.0, +1.0));

        objects.at(OBJ_TREE1).default_model = rotate(objects.at(OBJ_TREE1).default_model, PI, vec3(0.0, 0.0, 1.0));
        objects.at(OBJ_TREE1).default_model = rotate(objects.at(OBJ_TREE1).default_model, PIMED, vec3(1.0, 0.0, 0.0));

        objects.at(OBJ_TREE2).default_model = objects.at(OBJ_TREE1).default_model;
        objects.at(OBJ_TREE3).default_model = objects.at(OBJ_TREE1).default_model;
        objects.at(OBJ_TREE4).default_model = objects.at(OBJ_TREE1).default_model;
        objects.at(OBJ_TREE5).default_model = objects.at(OBJ_TREE1).default_model;
        objects.at(OBJ_TREE6).default_model = objects.at(OBJ_TREE1).default_model;
        objects.at(OBJ_TREE7).default_model = objects.at(OBJ_TREE1).default_model;
        objects.at(OBJ_TREE8).default_model = objects.at(OBJ_TREE1).default_model;
        objects.at(OBJ_TREE9).default_model = objects.at(OBJ_TREE1).default_model;
        objects.at(OBJ_TREE10).default_model = objects.at(OBJ_TREE1).default_model;

        objects.at(OBJ_TREE1).default_model = translate(objects.at(OBJ_TREE1).default_model,   vec3(06.0, 4.0, 4.0));
        objects.at(OBJ_TREE2).default_model = translate(objects.at(OBJ_TREE2).default_model,   vec3(12.0, 4.0, 4.0));
        objects.at(OBJ_TREE3).default_model = translate(objects.at(OBJ_TREE3).default_model,   vec3(18.0, 4.0, 4.0));
        objects.at(OBJ_TREE4).default_model = translate(objects.at(OBJ_TREE4).default_model,   vec3(24.0, 4.0, 4.0));
        objects.at(OBJ_TREE5).default_model = translate(objects.at(OBJ_TREE5).default_model,   vec3(30.0, 4.0, 4.0));
        objects.at(OBJ_TREE6).default_model = translate(objects.at(OBJ_TREE6).default_model,   vec3(36.0, 4.0, 4.0));
        objects.at(OBJ_TREE7).default_model = translate(objects.at(OBJ_TREE7).default_model,   vec3(42.0, 4.0, 4.0));
        objects.at(OBJ_TREE8).default_model = translate(objects.at(OBJ_TREE8).default_model,   vec3(48.0, 4.0, 4.0));
        objects.at(OBJ_TREE9).default_model = translate(objects.at(OBJ_TREE9).default_model,   vec3(54.0, 4.0, 4.0));
        objects.at(OBJ_TREE10).default_model = translate(objects.at(OBJ_TREE10).default_model, vec3(60.0, 4.0, 4.0));

        set_before_draw(vec_ptr(objects, OBJ_TREE1), __look_at_view);
        set_before_draw(vec_ptr(objects, OBJ_TREE2), __look_at_view);
        set_before_draw(vec_ptr(objects, OBJ_TREE3), __look_at_view);
        set_before_draw(vec_ptr(objects, OBJ_TREE4), __look_at_view);
        set_before_draw(vec_ptr(objects, OBJ_TREE5), __look_at_view);
        set_before_draw(vec_ptr(objects, OBJ_TREE6), __look_at_view);
        set_before_draw(vec_ptr(objects, OBJ_TREE7), __look_at_view);
        set_before_draw(vec_ptr(objects, OBJ_TREE8), __look_at_view);
        set_before_draw(vec_ptr(objects, OBJ_TREE9), __look_at_view);
        set_before_draw(vec_ptr(objects, OBJ_TREE10), __look_at_view);

        init_object_vector(objects);
}


/* Entry point: Initialize stuff and then enter mainloop. */
int
main()
{
        if (!glfwInit()) {
                fprintf(stderr, "Can not init glfw\n");
                exit(1);
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        // GLFWmonitor *monitor = glfwGetPrimaryMonitor(); // fullscreen
        GLFWmonitor *monitor = NULL; // floating (or not)
        GLFWwindow *share = NULL;
        GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Titulo", monitor, share);

        if (window == NULL) {
                perror("glfwCreateWindow");
                glfwTerminate(); // terminate initialized glfw
                return 1;
        }

        glfwMakeContextCurrent(window);
        glfwSetFramebufferSizeCallback(window, __framebuffer_size_callback);
#if defined(VSYNC)
        glfwSwapInterval(VSYNC);
#endif

        if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
                fprintf(stderr, "gladLoadGLLoader failed");
                glfwTerminate();
                return 1;
        }

        glfwSetCursorPosCallback(window, mouse_callback);

        /* Mouse stuff */

#if !defined(_WIN32) /* Why windows, why? */
        if (glfwRawMouseMotionSupported())
#endif
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

        glClearDepth(1.0f);
        glClearColor(BG_COLOR);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        show_help();

        init_objects();

        mainloop(window);
        glfwDestroyWindow(window);
        glfwTerminate();

        return 0;
}
