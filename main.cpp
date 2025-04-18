// clang-format off
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
|*                                                      *|  /
|*                                                      *| /
|*                                                      *|/
|* ---------------------------------------------------- */
// clang-format on

/* Enable VSync. FPS limited to screen refresh rate
 * (0: disable, 1: enable, undef: default) */
#include <cstdlib>
#include <glm/geometric.hpp>
#include <string>
#define VSYNC 0

/* Mouse sensibility */
#define MOUSE_SENS_X 6.0f
#define MOUSE_SENS_Y 6.0f

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
#error "Undefined target"
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
float camera_offset_y = 48.0f;
float camera_offset_x = 24.0f;
vec3 cameraPosition = vec3(0, 0, 0);
float interframe_time = 0;

enum {
        VIEW_3_PERSON,
        VIEW_1_PERSON,
        VIEW_NOFOLLOW,
} VIEW = VIEW_NOFOLLOW;

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
        float Ka[3] = { .2f, .2f, .2f }; // Luz ambiental
        float Ks[3] = { .4f, .4f, .4f }; // Reflectividad especular
        float Kd[3] = { 20.8f, 20.8f, 20.8f }; // Color difuso
        float Ke[3] = { .0f, .0f, .0f }; // Emisión (ninguna)
        float Ni = 1.0f; // Índice de refracción
        float d = 1.0f; // Opacidad
        unsigned int illum = 0;
        vector<unsigned int> textures;
        unsigned char *image = NULL;
        int height = 0;
        int width = 0;
        int comp = 0;
        unsigned int id = 0;

        Material(const char *name = "default")
        : name(name)
        {
        }

        GLuint
        load_texture(int how = 0)
        {
                unsigned int texture;
                if (image == NULL) {
                        printf("[ERROR] Material has no image data!\n");
                        return 0;
                }
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
                textures.push_back(texture);
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
          printable(_printable),
          color(_color),
          model(mat4(1.0f)),
          default_model(mat4(1.0f)),
          attached(),
          parent(nullptr),
          material(Material()),
          get_vao(get_vao_func)
        {
        }

        void show()
        {
                printable = true;
        }

        void hide()
        {
                printable = false;
        }

        Object &set_shader(GLuint shader)
        {
                shader_program = shader;
                return *this;
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

        Object &set_model(mat4 _model)
        {
                model = _model;
                return *this;
        }

        Object &set_before_draw_function(void (*_before_draw)(Object *))
        {
                before_draw = _before_draw;
                return *this;
        }

        unsigned int get_shader()
        {
                return shader_program;
        }

        Object &rotate(float angle, vec3 v)
        {
                model = glm::rotate(model, angle, v);
                return *this;
        }

        Object &translate(vec3 v)
        {
                model = glm::translate(model, v);
                return *this;
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
                glUniform1i(glGetUniformLocation(shader_program, "useTexture"), material.textures.size() > 0);
                glUniform1i(glGetUniformLocation(shader_program, "texture_count"), material.textures.size());
                if (colorLoc != -1)
                        glUniform3f(colorLoc, HexColor(color));

                for (int i = 0; i < material.textures.size(); ++i) {
                        unsigned int texture = material.textures.at(i);
                        if (glIsTexture(texture)) {
                                // printf("Using texture %d\n",material.texture);
                                glEnable(GL_TEXTURE_2D);
                                glActiveTexture(GL_TEXTURE0 + i);
                                textureLoc = glGetUniformLocation(shader_program, ("textures[" + std::to_string(i) + "]").c_str());
                                glUniform1i(textureLoc, i);
                                glBindTexture(GL_TEXTURE_2D, texture);

                        } else if (texture != 0) {
                                printf("Texture %d failed!\n", texture);
                                exit(texture);
                        }
                }
        }

        void
        draw_object(mat4 _model = mat4(1.0f))
        {
                static GLuint active_shader = 0;

                _model = _model * model;

                for (Object *attached_obj : attached) {
                        (*attached_obj).draw_object(_model);
                }

                if (!printable) return;
                if (before_draw) before_draw(this);

                if (active_shader != shader_program) {
                        glUseProgram(shader_program);
                        active_shader = shader_program;
                }

                set_camera(shader_program);
                set_texture_n_color();

                glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, value_ptr(_model));

                glBindVertexArray(vao);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                glDrawElements(GL_TRIANGLES, indexes_n, GL_UNSIGNED_INT, 0);

                glBindVertexArray(0);
                glBindTexture(GL_TEXTURE_2D, 0);
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

        bool collide(Object o)
        {
                vec3 self_pos = get_obj_absolute_position();
                vec3 obj_pos = o.get_obj_absolute_position();
                self_pos.y = 0;
                obj_pos.y = 0;
                float dist = glm::distance(self_pos, obj_pos);
                return false;
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

class Scene
{
    private:
        vector<Object> objects;

    public:
        Scene()
        {
        }

        vector<Object> scene_object_collisions(Object obj, vector<Object> other)
        {
                vector<Object> collisions;
                for (auto o : other) {
                        if (o.collide(obj))
                                collisions.push_back(o);
                }
                for (auto c : collisions)
                        printf("%s collide with %s\n", obj.get_name(), c.get_name());
                return collisions;
        }

        vector<Object> get_objects()
        {
                return objects;
        }

        Scene &add_object(Object &o)
        {
                objects.push_back(o);
                return *this;
        }
};

static Scene scene = Scene();

extern Object obj_grnd;
extern Object obj_base;
extern Object obj_head;
extern Object obj_w_fl;
extern Object obj_w_fr;
extern Object obj_w_bl;
extern Object obj_w_br;
extern Object obj_rotb;
extern Object obj_rots;
extern Object obj_palo;
extern Object obj_cube;
extern Object obj_ls_1;
extern Object obj_ls_2;
extern Object obj_tre1;
extern Object obj_tre2;
extern Object obj_tre3;
extern Object obj_tre4;
extern Object obj_tre5;
extern Object obj_tre6;
extern Object obj_tre7;
extern Object obj_tre8;
extern Object obj_tre9;
extern Object obj_tre0;


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
                cameraPosition = vec3(camera_offset_x, camera_offset_y, 0);
                cameraUp = vec3(0.0f, 1.0f, 0.0f);
                break;

        case VIEW_3_PERSON:
                m = obj_base.get_absolute_model();
                dirf = normalize(vec3(m[0]));
                cameraEye = obj_base.get_obj_absolute_position();
                cameraPosition = cameraEye - dirf * camera_offset_x + vec3(0.0f, camera_offset_y, 0.0f);
                cameraUp = vec3(m[1]);
                break;

        case VIEW_1_PERSON:
                m = obj_head.get_absolute_model();
                cameraPosition = vec3(m[3]);
                dirf = normalize(vec3(m[0]));
                cameraEye = cameraPosition + dirf;
                cameraUp = vec3(obj_base.get_model()[1]);
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
get_sphere_base_vao(GLuint *vao, GLuint *indexes_n)
{
        cube(vao, indexes_n, 1.0, 1.0, 1.0);
}

void
get_sphere_vao(GLuint *vao, GLuint *indexes_n)
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
process_mouse(GLFWwindow *window)
{
        static bool firstMouse = true;
        static float lastX;
        double _xpos, _ypos;

        glfwGetCursorPos(window, &_xpos, &_ypos);
        float xpos = static_cast<float>(_xpos);
        if (firstMouse) {
                lastX = xpos;
                firstMouse = false;
        }

        float xoffset = xpos - lastX;
        obj_base.rotate(MOUSE_SENS_X * (interframe_time) *xoffset, vec3(0, -1, 0));
        lastX = xpos;
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

                /* Check for collisions */
                if (scene.scene_object_collisions(obj_base, scene.get_objects()).size() > 0) {
                        moveSpeed = 0;
                }

                // move obj
                obj_base.translate(vec3(moveSpeed, 0, 0));
                // rotate wheel
                obj_w_fl.rotate(moveSpeed, vec3(0, 0, -1));
                obj_w_fr.rotate(moveSpeed, vec3(0, 0, -1));
                obj_w_bl.rotate(moveSpeed, vec3(0, 0, -1));
                obj_w_br.rotate(moveSpeed, vec3(0, 0, -1));
        }

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_RELEASE) {
                moveSpeed -= moveDec;
                if ((moveSpeed) < 0) {
                        moveSpeed = 0;
                } else {
                        // printf("SpeedDown %f\n", moveSpeed);
                        //   move obj
                        obj_base.translate(vec3(moveSpeed, 0, 0));
                        // rotate wheel
                        obj_w_fl.rotate(moveSpeed, vec3(0, 0, -1));
                        obj_w_fr.rotate(moveSpeed, vec3(0, 0, -1));
                        obj_w_bl.rotate(moveSpeed, vec3(0, 0, -1));
                        obj_w_br.rotate(moveSpeed, vec3(0, 0, -1));
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
                        obj_base.translate(vec3(-moveSpeed, 0, 0));
                        // rotate wheel
                        obj_w_fl.rotate(moveSpeed, vec3(0, 0, 1));
                        obj_w_fr.rotate(moveSpeed, vec3(0, 0, 1));
                        obj_w_bl.rotate(moveSpeed, vec3(0, 0, 1));
                        obj_w_br.rotate(moveSpeed, vec3(0, 0, 1));
                        moveSpeed = 0;
                }
        }

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                obj_base.rotate(rotateSpeed, vec3(0, 1, 0));
        }

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                obj_base.rotate(rotateSpeed, vec3(0, -1, 0));
        }

        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) {
                if (obj_rots.get_rotation().z < PIMED)
                        obj_rots.rotate(rotateSpeed, vec3(0, 0, 1));
        }

        if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) {
                if (obj_rots.get_rotation().z > -PIMED)
                        obj_rots.rotate(rotateSpeed, vec3(0, 0, -1));
        }

        if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS) {
                obj_rotb.rotate(rotateSpeed, vec3(0, 1, 0));
        }

        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
                obj_rotb.rotate(rotateSpeed, vec3(0, -1, 0));
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
                camera_offset_y += cameraSpeed;
        }

        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
                camera_offset_y -= cameraSpeed;
        }

        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
                camera_offset_x += cameraSpeed;
        }

        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
                camera_offset_x -= cameraSpeed;
        }
}

static void
framebuffer_size_callback(GLFWwindow *, int width, int height)
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
        last_tp = tp;

        if (t - last_time >= 1) {
                last_time = t;
#if defined(SHOW_FPS) && SHOW_FPS
                printf("[FPS] %u\n", fps);
#endif
                fps = 0;
        }
}

/* Main loop. Executed until window is closed. */
int
mainloop(GLFWwindow *window)
{
        while (!glfwWindowShouldClose(window)) {
                process_input(window);
                process_mouse(window);
                glfwPollEvents();

                fps();

                glClearColor(BG_COLOR);
                glClear(GL_COLOR_BUFFER_BIT);
                glClear(GL_DEPTH_BUFFER_BIT);

                set_light(&obj_ls_1, &obj_ls_2);

                obj_grnd.draw_object();
                obj_cube.draw_object();
                obj_base.draw_object();
                /* Every object attached to obj_base is automatically drawn */

                obj_tre1.draw_object();
                obj_tre2.draw_object();
                obj_tre3.draw_object();
                obj_tre4.draw_object();
                obj_tre5.draw_object();
                obj_tre6.draw_object();
                obj_tre7.draw_object();
                obj_tre8.draw_object();
                obj_tre9.draw_object();
                obj_tre0.draw_object();

                glfwSwapBuffers(window);
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
$(uniform sampler2D textures[16];)
$(uniform int texture_count;)

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

    $(float Kc = 50.0;)
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

    $(if (texture_count> 0))
    $({ )
        $(vec4 texColor = vec4(0, 0, 0, 0);)
        $(for (int i = 0; i<texture_count; i ++))
        $({ )
            $(switch (i))
            $({ )
                $(case 0 : texColor += texture(textures[0], TexCoord); break;)
                $(case 1 : texColor += texture(textures[1], TexCoord); break;)
                $(case 2 : texColor += texture(textures[2], TexCoord); break;)
                $(case 3 : texColor += texture(textures[3], TexCoord); break;)
                $( })
            $( })
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
look_to_camera(Object *self)
{
        self->look_at(cameraPosition);
}

Object obj_grnd = Object("ground", get_ground_vao);
Object obj_base = Object("base", get_base_vao);
Object obj_head = Object("head", get_head_vao);
Object obj_w_fl = Object("wheel_fl", get_wheel_vao, 0x444444);
Object obj_w_fr = Object("wheel_fr", get_wheel_vao, 0x444444);
Object obj_w_bl = Object("wheel_bl", get_wheel_vao, 0x444444);
Object obj_w_br = Object("wheel_br", get_wheel_vao, 0x444444);
Object obj_rotb = Object("circle_base", get_sphere_base_vao);
Object obj_rots = Object("circle", get_sphere_vao);
Object obj_palo = Object("palo", get_palo_vao);
Object obj_cube = Object("A", get_A_vao, 0xAACC00);
Object obj_ls_1 = Object("Light Spot 1", get_lightspot_vao, 0xFFFFFF, false); // light spot (where light is placed)
Object obj_ls_2 = Object("Light Spot 2", get_lightspot_vao, 0xFFFFFF, false);
Object obj_tre1 = Object("tree1", get_tree_vao);
Object obj_tre2 = Object("tree2", get_tree_vao);
Object obj_tre3 = Object("tree3", get_tree_vao);
Object obj_tre4 = Object("tree4", get_tree_vao);
Object obj_tre5 = Object("tree5", get_tree_vao);
Object obj_tre6 = Object("tree6", get_tree_vao);
Object obj_tre7 = Object("tree7", get_tree_vao);
Object obj_tre8 = Object("tree8", get_tree_vao);
Object obj_tre9 = Object("tree9", get_tree_vao);
Object obj_tre0 = Object("tree10", get_tree_vao);

void
init_objects()
{
        GLuint shader_program = setShaders_str(vertex_shader, fragment_shader);
        assert(shader_program > 0);

        obj_grnd.add_material_image("./textures/StripedAsphalt/Striped_Asphalt_ufoidcskw_1K_BaseColor.jpg");
        obj_grnd.add_material_image("./textures/random_texture_maps/ALPHA_090.png");
        obj_palo.add_material_image("./textures/bluePlastic/Scratched_Polypropylene_Plastic_schbehmp_1K_BaseColor.jpg");
        obj_base.add_material_image("./textures/marbleCheckeredFloor/Marble_Checkered_Floor_sescnen_1K_BaseColor.jpg");
        obj_tre1.add_material_image("./textures/tree2d.png");
        obj_head.set_material(obj_base.get_material());
        obj_rotb.set_material(obj_palo.get_material());
        obj_rots.set_material(obj_palo.get_material());
        obj_tre2.set_material(obj_tre1.get_material());
        obj_tre3.set_material(obj_tre1.get_material());
        obj_tre4.set_material(obj_tre1.get_material());
        obj_tre5.set_material(obj_tre1.get_material());
        obj_tre6.set_material(obj_tre1.get_material());
        obj_tre7.set_material(obj_tre1.get_material());
        obj_tre8.set_material(obj_tre1.get_material());
        obj_tre9.set_material(obj_tre1.get_material());
        obj_tre0.set_material(obj_tre1.get_material());

        obj_cube.translate(vec3(6.0, 3, 0.0));
        obj_base.rotate(PIMED, vec3(0.0, 1.0, 0.0));
        obj_base.translate(vec3(0.0, 1.39, 0.0));
        obj_head.translate(vec3(2.75, 2.0, 0.0));
        obj_w_fl.translate(vec3(2.5, -0.75, -1.5));
        obj_w_fr.translate(vec3(2.5, -0.75, +1.5));
        obj_w_bl.translate(vec3(-2.5, -0.75, -1.5));
        obj_w_br.translate(vec3(-2.5, -0.75, +1.5));
        obj_rotb.translate(vec3(-1.0, 0.75, 0.0));
        obj_rots.translate(vec3(0.0, 0.5, 0.0));
        obj_palo.translate(vec3(0.0, 2.75, 0.0));
        obj_ls_1.translate(vec3(0.0, 0.0, -1.0));
        obj_ls_2.translate(vec3(0.0, 0.0, +1.0));

        obj_tre1.rotate(PI, vec3(0.0, 0.0, 1.0)).rotate(PIMED, vec3(1.0, 0.0, 0.0));
        obj_tre2.set_model(obj_tre1.get_model()).translate(vec3(12.0, 4.0, 4.0));
        obj_tre3.set_model(obj_tre1.get_model()).translate(vec3(18.0, 4.0, 4.0));
        obj_tre4.set_model(obj_tre1.get_model()).translate(vec3(24.0, 4.0, 4.0));
        obj_tre5.set_model(obj_tre1.get_model()).translate(vec3(30.0, 4.0, 4.0));
        obj_tre6.set_model(obj_tre1.get_model()).translate(vec3(36.0, 4.0, 4.0));
        obj_tre7.set_model(obj_tre1.get_model()).translate(vec3(42.0, 4.0, 4.0));
        obj_tre8.set_model(obj_tre1.get_model()).translate(vec3(48.0, 4.0, 4.0));
        obj_tre9.set_model(obj_tre1.get_model()).translate(vec3(54.0, 4.0, 4.0));
        obj_tre0.set_model(obj_tre1.get_model()).translate(vec3(60.0, 4.0, 4.0));
        obj_tre1.translate(vec3(06.0, 4.0, 4.0));

        obj_tre1.set_before_draw_function(look_to_camera);
        obj_tre2.set_before_draw_function(look_to_camera);
        obj_tre3.set_before_draw_function(look_to_camera);
        obj_tre4.set_before_draw_function(look_to_camera);
        obj_tre5.set_before_draw_function(look_to_camera);
        obj_tre6.set_before_draw_function(look_to_camera);
        obj_tre7.set_before_draw_function(look_to_camera);
        obj_tre8.set_before_draw_function(look_to_camera);
        obj_tre9.set_before_draw_function(look_to_camera);
        obj_tre0.set_before_draw_function(look_to_camera);

        obj_base.attach(&obj_head);
        obj_base.attach(&obj_w_fl);
        obj_base.attach(&obj_w_fr);
        obj_base.attach(&obj_w_bl);
        obj_base.attach(&obj_w_br);
        obj_base.attach(&obj_rotb);
        obj_rotb.attach(&obj_rots);
        obj_rots.attach(&obj_palo);
        obj_head.attach(&obj_ls_1);
        obj_head.attach(&obj_ls_2);

        obj_grnd.set_shader(shader_program).init();
        obj_base.set_shader(shader_program).init();
        obj_head.set_shader(shader_program).init();
        obj_w_fl.set_shader(shader_program).init();
        obj_w_fr.set_shader(shader_program).init();
        obj_w_bl.set_shader(shader_program).init();
        obj_w_br.set_shader(shader_program).init();
        obj_rotb.set_shader(shader_program).init();
        obj_rots.set_shader(shader_program).init();
        obj_palo.set_shader(shader_program).init();
        obj_cube.set_shader(shader_program).init();
        obj_ls_1.set_shader(shader_program).init();
        obj_ls_2.set_shader(shader_program).init();
        obj_tre1.set_shader(shader_program).init();
        obj_tre2.set_shader(shader_program).init();
        obj_tre3.set_shader(shader_program).init();
        obj_tre4.set_shader(shader_program).init();
        obj_tre5.set_shader(shader_program).init();
        obj_tre6.set_shader(shader_program).init();
        obj_tre7.set_shader(shader_program).init();
        obj_tre8.set_shader(shader_program).init();
        obj_tre9.set_shader(shader_program).init();
        obj_tre0.set_shader(shader_program).init();
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

        printf("GLFW version: %s\n", glfwGetVersionString());
        switch (glfwGetPlatform()) {
        case GLFW_PLATFORM_ERROR:
                printf("GLFW Platform: Error\n");
                break;
        case GLFW_PLATFORM_NULL:
                printf("GLFW Platform: Null\n");
                break;
        case GLFW_PLATFORM_X11:
                printf("GLFW Platform: x11\n");
                break;
        case GLFW_PLATFORM_WAYLAND:
                printf("GLFW Platform: wayland\n");
                break;
        default:
                printf("GLFW Platform: Other\n");
                break;
        }


        glfwMakeContextCurrent(window);
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

        if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
                fprintf(stderr, "gladLoadGLLoader failed");
                glfwTerminate();
                return 1;
        }


#if defined(VSYNC)
        glfwSwapInterval(VSYNC);
#endif

        /* Mouse stuff */
        if (glfwRawMouseMotionSupported())
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        glClearDepth(1.0f);
        glClearColor(BG_COLOR);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        show_help();

        init_objects();
        mainloop(window);

        glfwDestroyWindow(window);
        glfwTerminate();

        return 0;
}
