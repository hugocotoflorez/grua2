    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * /|
   /                                                        / |
  /                                                        /  |
 / ____________________________________________________   /   |
|* Grua (texturas + iluminacion). Practica COGA 2025    *|    |
|* ---------------------------------------------------- *|    |
|* Autores:                                             *|    |
|*      Hugo Coto Florez                                *|    |
|*      Guillermo Fernandez                             *|    |
|*                                                      *|    |
|* License: licenseless                                 *|    /
|*                                                      *|   /
|* Version 0.02: Reimplementacion con clases.           *|  /
|* Version 0.<2: Sigue visible en ./deprecated          *| /
|*                                                      *|/
|* ---------------------------------------------------- */

/* Enable VSync. FPS limited to screen refresh rate
 * (0: disable, 1: enable, undef: default) */
#define VSYNC 0

/* Show fps if SHOW_FPS is defined and not 0 */
#define SHOW_FPS 0

#if defined(linux)
#include <glad/glad.h>

#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <glm/ext.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/glm.hpp>

#else
static_assert(0, "Undefined target");
#endif

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

// #include "../thirdparty/frog/frog.h"

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

#define BG_COLOR HexColor(0x87CEEB), 1.0

#define PI 3.1416f
#define PIMED 1.5708f

#define Point(a, b, c) a, b, c
#define Face4(a, b, c, d) a, b, c, a, c, d
#define vec_ptr(vec, i) ((vec).data() + (i))
#define SIZE(arr) (sizeof((arr)) / sizeof(*(arr)))
#define Texture(a, b) a, b
#define Normal(a, b, c) a, b, c

#define $(a) #a "\n"

GLuint WIDTH = 640;
GLuint HEIGHT = 480;

vec3 lightPos = vec3(0.0f, 0.0f, 0.0f); // Posición de la luz en el mundo
vec3 lightColor = vec3(1.0f, 1.0f, 1.0f); // Blanco
float lightIntensity = 4.0f;

unsigned int c_lock = 0;
float camera_pos_y = 48.0f;
float camera_pos_x = 24.0f;
vec3 cameraPosition = vec3(0, 0, 0);

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
                // printf("Using shader %d\n", shader);
                global_shader = shader;
        }

        return global_shader;
}


struct gvopts {
        GLuint vertex_start, vertex_coords;
        GLuint texture_start, texture_coords;
        GLuint normal_start, normal_coords;
        GLuint padd;
        bool use_vertex, use_texture, use_normal;
};

class Material
{
    public:
        const char *name;
        float Ns = 200.0f; // Brillo especular
        float Ka[3] = { 0.2f, 0.2f, 0.2f }; // Luz ambiental
        float Ks[3] = { 0.4f, 0.4f, 0.4f }; // Reflectividad especular
        float Kd[3] = { 20.8f, 20.8f, 20.8f }; // Color difuso
        float Ke[3] = { 0.0f, 0.0f, 0.0f }; // Emisión (ninguna)
        float Ni = 1.0f; // Índice de refracción
        float d = 1.0f; // Opacidad
        unsigned int illum = 0;
        unsigned int texture = 0;
        unsigned char *image = NULL;
        int height = 0;
        int width = 0;
        int comp = 0;
        unsigned int id = 0;

        Material(const char *name = "default") : name(name)
        {
        }

        GLuint
        load_texture(int how = 0)
        {
                if (texture > 0) {
                        // printf("Texture loaded yet\n");
                        return texture;
                }

                if (image == NULL) {
                        // printf("Material has no image data!\n");
                        return 0;
                }
                // printf("loading texture\n");
                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, how ?: GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, how ?: GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

                assert(width > 0 && height > 0);

                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
                glGenerateMipmap(GL_TEXTURE_2D);

                glBindTexture(GL_TEXTURE_2D, 0);

                assert(texture > 0);
                // printf("texture loaded\n");
                return texture;
        }

        void
        add_material_image(const char *path, int how = 0)
        {
                stbi_set_flip_vertically_on_load(1);

                image = stbi_load(path, &width, &height, &comp, STBI_rgb_alpha);

                if (image == NULL) {
                        fprintf(stderr, "Failed to load texture '%s'\n", path);
                        exit(0);
                }
                load_texture(how);
                stbi_image_free(image);
        }
};

void set_camera(int);

class Object
{
    private:
        const char *name;
        GLuint vao = 0;
        GLuint indexes_n = 0;
        bool printable;
        int color;
        mat4 model; // relative model
        mat4 default_model; // default starting relative model
        vector<Object *> attached;
        Object *parent;
        Material material;
        unsigned int shader_program;
        void (*get_vao)(GLuint *, GLuint *);
        void (*before_draw)(Object *);

    public:
        Object(const char *name, void (*get_vao_func)(GLuint *, GLuint *),
               int _color = 0xFFFFFF,
               bool _printable = true)
        : name(name),
          color(_color),
          printable(_printable),
          get_vao(get_vao_func),
          material(Material()),
          attached(),
          parent(nullptr),
          model(mat4(1.0f)),
          default_model(mat4(1.0f))
        {
                shader_program = use_global_shader(0);
        }

        void show()
        {
                printable = true;
        }

        void hide()
        {
                printable = false;
        }

        const char *get_name()
        {
                return name;
        }

        Material get_material()
        {
                return material;
        }

        void set_material(Material _material)
        {
                material = _material;
        }

        vec3
        get_position()
        {
                return vec3(model[3]);
        }

        mat4
        get_absolute_model()
        {
                if (parent) {
                        return parent->get_absolute_model() * model;
                }
                return model;
        }

        vec3
        get_obj_absolute_position()
        {
                return vec3(get_absolute_model()[3]);
        }

        mat3
        get_rotation_matrix()
        {
                return mat3(model);
        }

        Object *
        add_material_image(const char *path, int how = 0)
        {
                material.add_material_image(path, how);
                return this;
        }

        vec3
        get_rotation()
        {
                return eulerAngles(quat_cast(mat3(model)));
        }

        mat3 get_default_rotation_matrix()
        {
                return mat3(default_model);
        }

        mat4 get_model()
        {
                return model;
        }

        void set_model(mat4 _model)
        {
                model = _model;
        }

        void set_before_draw(void (*_before_draw)(Object *))
        {
                before_draw = _before_draw;
        }

        unsigned int get_shader()
        {
                return shader_program;
        }

        Object *rotate(float angle, vec3 v)
        {
                model = glm::rotate(model, angle, v);
                return this;
        }

        Object *translate(vec3 v)
        {
                model = glm::translate(model, v);
                return this;
        }

        Object *push(vector<Object *> *objs)
        {
                objs->push_back(this);
                return this;
        }

        void
        look_at(vec3 view_pos)
        {
                vec3 obj_pos = get_position();
                mat3 rotation = get_default_rotation_matrix();
                vec3 dir = normalize(vec3(view_pos.x - obj_pos.x, 0.0, view_pos.z - obj_pos.z));
                float angle = atan2(dir.x, dir.z);
                mat4 _model = mat4(1.0f);
                _model = glm::translate(_model, obj_pos);
                _model = glm::rotate(_model, angle, vec3(0.0, 1.0, 0.0));
                _model *= mat4(rotation);
                model = _model;
        }

        void
        set_texture_n_color()
        {
                GLint colorLoc;
                GLint textureLoc;
                colorLoc = glGetUniformLocation(shader_program, "color");
                textureLoc = glGetUniformLocation(shader_program, "texture1");
                if (colorLoc != -1)
                        glUniform3f(colorLoc, HexColor(color));

                glUniform1i(glGetUniformLocation(shader_program, "useTexture"), material.texture != 0);
                if (textureLoc != -1 && glIsTexture(material.texture)) {
                        // printf("Using texture %d\n",material.texture);
                        glEnable(GL_TEXTURE_2D);
                        glActiveTexture(GL_TEXTURE0);
                        glUniform1i(textureLoc, 0);
                        glBindTexture(GL_TEXTURE_2D, material.texture);

                } else if (material.texture != 0) {
                        // printf("Texture %d failed!\n", object.material.texture);
                        // exit(object.material.texture);
                }
        }

        void
        draw_object(mat4 _model = mat4(1.0f))
        {
                static GLuint active_shader = 0;
                GLuint modelLoc;

                // printf("In draw_object\n");

                // get obj model
                _model = _model * model;

                // printf("1\n");
                // printf("vector length: %zd\n", attached.size());
                for (Object *attached_obj : attached) {
                        assert(attached_obj != nullptr);
                        // printf("Attached object: %s\n", (*attached_obj).get_name());
                        (*attached_obj).draw_object(_model);
                }
                // printf("2\n");

                if (!printable)
                        return;

                if (before_draw) {
                        before_draw(this);
                }

                if (active_shader != shader_program) {
                        // printf("Shader set to %d\n", shader_program);
                        glUseProgram(shader_program);
                        active_shader = shader_program;
                }
                // printf("4\n");

                set_camera(shader_program);
                // printf("5\n");
                set_texture_n_color();
                // printf("6\n");

                modelLoc = glGetUniformLocation(shader_program, "model");
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(_model));

                glBindVertexArray(vao);
                glDrawElements(GL_TRIANGLES, indexes_n, GL_UNSIGNED_INT, 0);

                glBindVertexArray(0);
                glBindTexture(GL_TEXTURE_2D, 0);
                // printf("7\n");
        }

        void
        attach(Object *child)
        {
                if (child->parent)
                        return;
                // printf("Vec Size before attach: %zd\n", attached.size());
                attached.push_back(child);
                child->parent = this;
                // printf("Vec Size after attach: %zd\n", attached.size());
        }

        void
        set_before_draw(Object *obj, void (*func)(Object *))
        {
                obj->before_draw = func;
        }

        void delete_vao()
        {
                glDeleteVertexArrays(1, &vao);
        }

        Object *init()
        {
                get_vao(&vao, &indexes_n);
                default_model = model;
                return this;
        }
};


vector<Object *> objects;

void
set_camera(int shader)
{
        vec3 cameraEye;
        vec3 cameraUp;
        mat4 m;
        vec3 dirf;
        mat4 view;
        GLuint viewLoc = glGetUniformLocation(shader, "view");
        GLuint projectionLoc = glGetUniformLocation(shader, "projection");
        mat4 projection = perspective(radians(45.0f), (float) WIDTH / (float) HEIGHT, 0.1f, 100.0f);

        switch (VIEW) {
        case VIEW_NOFOLLOW:
                cameraEye = vec3(0, 0, 0);
                cameraPosition = vec3(camera_pos_x, camera_pos_y, 0);
                cameraUp = vec3(0.0f, 1.0f, 0.0f);
                break;

        case VIEW_3_PERSON:
                m = objects.at(OBJ_BASE)->get_absolute_model();
                dirf = normalize(vec3(m[0]));
                cameraEye = objects.at(OBJ_BASE)->get_obj_absolute_position();
                cameraPosition = cameraEye - dirf * camera_pos_x + vec3(0.0f, camera_pos_y, 0.0f);
                cameraUp = vec3(m[1]);
                break;

        case VIEW_1_PERSON:
                m = objects.at(OBJ_HEAD)->get_absolute_model();
                cameraPosition = vec3(m[3]);
                dirf = normalize(vec3(m[0]));
                cameraEye = cameraPosition + dirf;
                cameraUp = vec3(objects.at(OBJ_BASE)->get_model()[1]);
                break;
        }

        view = lookAt(cameraPosition, cameraEye, cameraUp);

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, value_ptr(projection));
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
                objects.at(OBJ_BASE)->translate(vec3(moveSpeed, 0, 0));
                // rotate wheel
                objects.at(OBJ_WHEEL_FL)->rotate(moveSpeed, vec3(0, 0, -1));
                objects.at(OBJ_WHEEL_FR)->rotate(moveSpeed, vec3(0, 0, -1));
                objects.at(OBJ_WHEEL_BL)->rotate(moveSpeed, vec3(0, 0, -1));
                objects.at(OBJ_WHEEL_BR)->rotate(moveSpeed, vec3(0, 0, -1));
        }

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_RELEASE) {
                moveSpeed -= moveDec;
                if ((moveSpeed) < 0) {
                        moveSpeed = 0;
                } else {
                        // printf("SpeedDown %f\n", moveSpeed);
                        //   move obj
                        objects.at(OBJ_BASE)->translate(vec3(moveSpeed, 0, 0));
                        // rotate wheel
                        objects.at(OBJ_WHEEL_FL)->rotate(moveSpeed, vec3(0, 0, -1));
                        objects.at(OBJ_WHEEL_FR)->rotate(moveSpeed, vec3(0, 0, -1));
                        objects.at(OBJ_WHEEL_BL)->rotate(moveSpeed, vec3(0, 0, -1));
                        objects.at(OBJ_WHEEL_BR)->rotate(moveSpeed, vec3(0, 0, -1));
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
                        objects.at(OBJ_BASE)->translate(vec3(-moveSpeed, 0, 0));
                        // rotate wheel
                        objects.at(OBJ_WHEEL_FL)->rotate(moveSpeed, vec3(0, 0, 1));
                        objects.at(OBJ_WHEEL_FR)->rotate(moveSpeed, vec3(0, 0, 1));
                        objects.at(OBJ_WHEEL_BL)->rotate(moveSpeed, vec3(0, 0, 1));
                        objects.at(OBJ_WHEEL_BR)->rotate(moveSpeed, vec3(0, 0, 1));
                        moveSpeed = 0;
                }
        }

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                objects.at(OBJ_BASE)->rotate(rotateSpeed, vec3(0, 1, 0));
        }

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                objects.at(OBJ_BASE)->rotate(rotateSpeed, vec3(0, -1, 0));
        }

        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) {
                if (objects.at(OBJ_SPHERE)->get_rotation().z < PIMED)
                        objects.at(OBJ_SPHERE)->rotate(rotateSpeed, vec3(0, 0, 1));
        }

        if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) {
                if (objects.at(OBJ_SPHERE)->get_rotation().z > -PIMED)
                        objects.at(OBJ_SPHERE)->rotate(rotateSpeed, vec3(0, 0, -1));
        }

        if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS) {
                objects.at(OBJ_SPHERE_BASE)->rotate(rotateSpeed, vec3(0, 1, 0));
        }

        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
                objects.at(OBJ_SPHERE_BASE)->rotate(rotateSpeed, vec3(0, -1, 0));
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


void
set_light(Object *light1, Object *light2)
{
        mat4 model = light1->get_absolute_model();
        vec3 local_light_pos = vec3(0.0f, 0.0f, 0.0f); // Posición relativa dentro del objeto
        lightPos = vec3(model * vec4(local_light_pos, 1.0f)); // Aplicar la transformación completa
        vec3 dirf = normalize(vec3(model[0]));
        glUniform3fv(glGetUniformLocation(light1->get_shader(), "lightPos1"), 1, value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(light1->get_shader(), "lightDir1"), 1, value_ptr(dirf));

        model = light2->get_absolute_model();
        local_light_pos = vec3(0.0f, 0.0f, 0.0f); // Posición relativa dentro del objeto
        lightPos = vec3(model * vec4(local_light_pos, 1.0f)); // Aplicar la transformación completa
        dirf = normalize(vec3(model[0]));
        glUniform3fv(glGetUniformLocation(light1->get_shader(), "lightPos2"), 1, value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(light1->get_shader(), "lightDir2"), 1, value_ptr(dirf));

        vec3 viewPos = cameraPosition;
        // printf("Light pos: %f, %f, %f\n", lightPos.x, lightPos.y, lightPos.z);
        glUniform3fv(glGetUniformLocation(light1->get_shader(), "viewPos"), 1, value_ptr(viewPos));
        glUniform3fv(glGetUniformLocation(light1->get_shader(), "lightColor"), 1, value_ptr(lightColor * lightIntensity));
        glUniform3fv(glGetUniformLocation(light1->get_shader(), "Ka"), 1, light1->get_material().Ka);
        glUniform3fv(glGetUniformLocation(light1->get_shader(), "Ks"), 1, light1->get_material().Ks);
        glUniform3fv(glGetUniformLocation(light1->get_shader(), "Kd"), 1, light1->get_material().Kd);
        glUniform3fv(glGetUniformLocation(light1->get_shader(), "Ke"), 1, light1->get_material().Ke);
        glUniform1f(glGetUniformLocation(light1->get_shader(), "Ns"), light1->get_material().Ns);
        glUniform1f(glGetUniformLocation(light1->get_shader(), "Ni"), light1->get_material().Ni);
        glUniform1f(glGetUniformLocation(light1->get_shader(), "d"), light1->get_material().d);


        /* Not suported in windows? */
        // glShadeModel(GL_SMOOTH);
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

        objects.at(OBJ_BASE)->rotate(rotateSpeed * xoffset, vec3(0, -1, 0));
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
                // printf("In mainloop\n");
                process_input(window);
                // printf("Input processed\n");
                fps();
                // printf("fps got\n");

                glClearColor(BG_COLOR);
                glClear(GL_COLOR_BUFFER_BIT);
                glClear(GL_DEPTH_BUFFER_BIT);

                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                // printf("setting lights\n");
                set_light(objects.at(OBJ_LIGHT_SPOT1), objects.at(OBJ_LIGHT_SPOT2));
                // printf("lights set\n");

                objects.at(OBJ_GROUND)->draw_object();
                objects.at(OBJ_A)->draw_object();
                objects.at(OBJ_BASE)->draw_object();

                objects.at(OBJ_TREE1)->draw_object();
                objects.at(OBJ_TREE2)->draw_object();
                objects.at(OBJ_TREE3)->draw_object();
                objects.at(OBJ_TREE4)->draw_object();
                objects.at(OBJ_TREE5)->draw_object();
                objects.at(OBJ_TREE6)->draw_object();
                objects.at(OBJ_TREE7)->draw_object();
                objects.at(OBJ_TREE8)->draw_object();
                objects.at(OBJ_TREE9)->draw_object();
                objects.at(OBJ_TREE10)->draw_object();
                /* Every object attached to OBJ_BASE is automatically drawn */

                glfwSwapBuffers(window);
                glfwPollEvents();
        }

        for (auto obj : objects) {
                obj->delete_vao();
        }

        return 0;
}

static const char vertex_shader[] =
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

static const char fragment_shader[] =
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
    $(float ambientStrength = 0.8;)
    $(vec3 ambient = ambientStrength * lightColor;)
    $(vec3 light = ambient * Ka;)
    $(vec3 specular = vec3(0, 0, 0);)
    $(vec3 lightDirNorm1 = normalize(lightPos1 - FragPos);)
    $(vec3 lightDirNorm2 = normalize(lightPos2 - FragPos);)
    $(float distance1 = length(lightPos1 - FragPos);)
    $(float distance2 = length(lightPos2 - FragPos);)

    $(float Kc = 2.0;)
    $(float Kl = 1.2;)
    $(float Kq = 0.09;)

    $(float attenuation1 = 1.0 /(Kc + Kl * distance1 + Kq *(distance1 * distance1));)
    $(float specularStrength = 0.5;)
    $(vec3 fd1 = normalize(FragPos - lightPos1);)
    $(vec3 fd2 = normalize(FragPos - lightPos2);)

    $(if (acos(dot(fd1, lightDirection1))<radians(15.0)))
    $({ )
        $(float diff = max(dot(norm, lightDirNorm1), 0.0);)
        $(vec3 diffuse = diff * Kd * attenuation1 * lightColor;)
        $(light += diffuse;)
        $(vec3 viewDir = normalize(viewPos - FragPos);)
        $(vec3 reflectDir = reflect(- lightDirNorm1, norm);)
        $(float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);)
        $(specular = specularStrength * spec * lightColor * attenuation1;)
        $( })

    $(else if (acos(dot(fd2, lightDirection2))<radians(15.0)))
    $({ )
        $(float diff = max(dot(norm, lightDirNorm2), 0.0);)
        $(vec3 diffuse = diff * Kd * attenuation1 * lightColor;)
        $(light += diffuse;)
        $(vec3 viewDir = normalize(viewPos - FragPos);)
        $(vec3 reflectDir = reflect(- lightDirNorm2, norm);)
        $(float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);)
        $(specular = specularStrength * spec * lightColor * attenuation1;)
        $( })

    $(if (useTexture == 1))
    $({ )
        $(vec4 texColor = texture(texture1, TexCoord);)
        $(if (texColor.a<0.1))
        $({ )
            $(discard;)
            $( })
        $(FragColor = vec4(light + specular, 1.0) * texColor;)
        $( })
    $(else)
    $({ )
        $(FragColor = vec4(light * color + specular, 1.0);)
        $( })
    $( }) "\n\0";


void
init_object_vector(vector<Object *> &objs)
{
        for (auto o : objs) {
                o->init();
        }
}

void
__look_at_view(Object *self)
{
        // printf("Object %s looking to camera\n", self->get_name());
        self->look_at(cameraPosition);
}

void
init_objects()
{
        GLuint shader_program = setShaders_str(vertex_shader, fragment_shader);
        assert(shader_program > 0);

        use_global_shader(shader_program);

        static Object ground = Object("ground", get_ground_vao);
        static Object base = Object("base", get_base_vao);
        static Object head = Object("head", get_head_vao);
        static Object wheel_fl = Object("wheel_fl", get_wheel_vao, 0x444444);
        static Object wheel_fr = Object("wheel_fr", get_wheel_vao, 0x444444);
        static Object wheel_bl = Object("wheel_bl", get_wheel_vao, 0x444444);
        static Object wheel_br = Object("wheel_br", get_wheel_vao, 0x444444);
        static Object circle_base = Object("circle_base", get_circle_base_vao);
        static Object circle = Object("circle", get_circle_vao);
        static Object palo = Object("palo", get_palo_vao);
        static Object A = Object("A", get_A_vao, 0xAACC00);
        static Object Light_Spot_1 = Object("Light Spot 1", get_lightspot_vao, 0xFFFFFF, false);
        static Object Light_Spot_2 = Object("Light Spot 2", get_lightspot_vao, 0xFFFFFF, false);
        static Object tree1 = Object("tree1", get_tree_vao);
        static Object tree2 = Object("tree2", get_tree_vao);
        static Object tree3 = Object("tree3", get_tree_vao);
        static Object tree4 = Object("tree4", get_tree_vao);
        static Object tree5 = Object("tree5", get_tree_vao);
        static Object tree6 = Object("tree6", get_tree_vao);
        static Object tree7 = Object("tree7", get_tree_vao);
        static Object tree8 = Object("tree8", get_tree_vao);
        static Object tree9 = Object("tree9", get_tree_vao);
        static Object tree10 = Object("tree10", get_tree_vao);
        ground.push(&objects);
        base.push(&objects);
        head.push(&objects);
        wheel_fl.push(&objects);
        wheel_fr.push(&objects);
        wheel_bl.push(&objects);
        wheel_br.push(&objects);
        circle_base.push(&objects);
        circle.push(&objects);
        palo.push(&objects);
        A.push(&objects);
        Light_Spot_1.push(&objects);
        Light_Spot_2.push(&objects);
        tree1.push(&objects);
        tree2.push(&objects);
        tree3.push(&objects);
        tree4.push(&objects);
        tree5.push(&objects);
        tree6.push(&objects);
        tree7.push(&objects);
        tree8.push(&objects);
        tree9.push(&objects);
        tree10.push(&objects);

        assert(objects.size() > 0);

        objects.at(OBJ_GROUND)->add_material_image("./textures/StripedAsphalt/Striped_Asphalt_ufoidcskw_1K_BaseColor.jpg");
        objects.at(OBJ_PALO)->add_material_image("./textures/bluePlastic/Scratched_Polypropylene_Plastic_schbehmp_1K_BaseColor.jpg");
        objects.at(OBJ_BASE)->add_material_image("./textures/marbleCheckeredFloor/Marble_Checkered_Floor_sescnen_1K_BaseColor.jpg");
        objects.at(OBJ_TREE1)->add_material_image("./textures/tree2d.png", GL_CLAMP_TO_EDGE);

        objects.at(OBJ_HEAD)->set_material(objects.at(OBJ_BASE)->get_material());
        objects.at(OBJ_SPHERE_BASE)->set_material(objects.at(OBJ_PALO)->get_material());
        objects.at(OBJ_SPHERE)->set_material(objects.at(OBJ_PALO)->get_material());

        objects.at(OBJ_TREE2)->set_material(objects.at(OBJ_TREE1)->get_material());
        objects.at(OBJ_TREE3)->set_material(objects.at(OBJ_TREE1)->get_material());
        objects.at(OBJ_TREE4)->set_material(objects.at(OBJ_TREE1)->get_material());
        objects.at(OBJ_TREE5)->set_material(objects.at(OBJ_TREE1)->get_material());
        objects.at(OBJ_TREE6)->set_material(objects.at(OBJ_TREE1)->get_material());
        objects.at(OBJ_TREE7)->set_material(objects.at(OBJ_TREE1)->get_material());
        objects.at(OBJ_TREE8)->set_material(objects.at(OBJ_TREE1)->get_material());
        objects.at(OBJ_TREE9)->set_material(objects.at(OBJ_TREE1)->get_material());
        objects.at(OBJ_TREE10)->set_material(objects.at(OBJ_TREE1)->get_material());

        // A
        objects.at(OBJ_A)->translate(vec3(6.0, 3, 0.0));

        // Base
        objects.at(OBJ_BASE)->rotate(PIMED, vec3(0.0, 1.0, 0.0));
        objects.at(OBJ_BASE)->translate(vec3(0.0, 1.39, 0.0));

        // Head
        objects.at(OBJ_BASE)->attach(objects.at(OBJ_HEAD));
        objects.at(OBJ_HEAD)->translate(vec3(2.75, 2.0, 0.0));

        // wheel_fl
        objects.at(OBJ_BASE)->attach(objects.at(OBJ_WHEEL_FL));
        objects.at(OBJ_WHEEL_FL)->translate(vec3(2.5, -0.75, -1.5));

        // wheel_fr
        objects.at(OBJ_BASE)->attach(objects.at(OBJ_WHEEL_FR));
        objects.at(OBJ_WHEEL_FR)->translate(vec3(2.5, -0.75, +1.5));

        // wheel_bl
        objects.at(OBJ_BASE)->attach(objects.at(OBJ_WHEEL_BL));
        objects.at(OBJ_WHEEL_BL)->translate(vec3(-2.5, -0.75, -1.5));

        // wheel_br
        objects.at(OBJ_BASE)->attach(objects.at(OBJ_WHEEL_BR));
        objects.at(OBJ_WHEEL_BR)->translate(vec3(-2.5, -0.75, +1.5));

        // sphere base
        objects.at(OBJ_BASE)->attach(objects.at(OBJ_SPHERE_BASE));
        objects.at(OBJ_SPHERE_BASE)->translate(vec3(-1.0, 0.75, 0.0));

        // sphere
        objects.at(OBJ_SPHERE_BASE)->attach(objects.at(OBJ_SPHERE));
        objects.at(OBJ_SPHERE)->translate(vec3(0.0, 0.5, 0.0));

        // palo
        objects.at(OBJ_SPHERE)->attach(objects.at(OBJ_PALO));
        objects.at(OBJ_PALO)->translate(vec3(0.0, 2.75, 0.0));

        // light spot (where light is placed)
        objects.at(OBJ_HEAD)->attach(objects.at(OBJ_LIGHT_SPOT1));
        objects.at(OBJ_LIGHT_SPOT1)->translate(vec3(0.0, 0.0, -1.0));

        objects.at(OBJ_HEAD)->attach(objects.at(OBJ_LIGHT_SPOT2));
        objects.at(OBJ_LIGHT_SPOT2)->translate(vec3(0.0, 0.0, +1.0));

        objects.at(OBJ_TREE1)->rotate(PI, vec3(0.0, 0.0, 1.0));
        objects.at(OBJ_TREE1)->rotate(PIMED, vec3(1.0, 0.0, 0.0));

        objects.at(OBJ_TREE2)->set_model(objects.at(OBJ_TREE1)->get_model());
        objects.at(OBJ_TREE3)->set_model(objects.at(OBJ_TREE1)->get_model());
        objects.at(OBJ_TREE4)->set_model(objects.at(OBJ_TREE1)->get_model());
        objects.at(OBJ_TREE5)->set_model(objects.at(OBJ_TREE1)->get_model());
        objects.at(OBJ_TREE6)->set_model(objects.at(OBJ_TREE1)->get_model());
        objects.at(OBJ_TREE7)->set_model(objects.at(OBJ_TREE1)->get_model());
        objects.at(OBJ_TREE8)->set_model(objects.at(OBJ_TREE1)->get_model());
        objects.at(OBJ_TREE9)->set_model(objects.at(OBJ_TREE1)->get_model());
        objects.at(OBJ_TREE10)->set_model(objects.at(OBJ_TREE1)->get_model());

        objects.at(OBJ_TREE1)->translate(vec3(06.0, 4.0, 4.0));
        objects.at(OBJ_TREE2)->translate(vec3(12.0, 4.0, 4.0));
        objects.at(OBJ_TREE3)->translate(vec3(18.0, 4.0, 4.0));
        objects.at(OBJ_TREE4)->translate(vec3(24.0, 4.0, 4.0));
        objects.at(OBJ_TREE5)->translate(vec3(30.0, 4.0, 4.0));
        objects.at(OBJ_TREE6)->translate(vec3(36.0, 4.0, 4.0));
        objects.at(OBJ_TREE7)->translate(vec3(42.0, 4.0, 4.0));
        objects.at(OBJ_TREE8)->translate(vec3(48.0, 4.0, 4.0));
        objects.at(OBJ_TREE9)->translate(vec3(54.0, 4.0, 4.0));
        objects.at(OBJ_TREE10)->translate(vec3(60.0, 4.0, 4.0));

        objects.at(OBJ_TREE1)->set_before_draw(__look_at_view);
        objects.at(OBJ_TREE2)->set_before_draw(__look_at_view);
        objects.at(OBJ_TREE3)->set_before_draw(__look_at_view);
        objects.at(OBJ_TREE4)->set_before_draw(__look_at_view);
        objects.at(OBJ_TREE5)->set_before_draw(__look_at_view);
        objects.at(OBJ_TREE6)->set_before_draw(__look_at_view);
        objects.at(OBJ_TREE7)->set_before_draw(__look_at_view);
        objects.at(OBJ_TREE8)->set_before_draw(__look_at_view);
        objects.at(OBJ_TREE9)->set_before_draw(__look_at_view);
        objects.at(OBJ_TREE10)->set_before_draw(__look_at_view);

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

        if (glfwRawMouseMotionSupported())
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
