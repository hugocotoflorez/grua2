/******************************************
  Lectura y Creacion de Shaders
*******************************************/

#include <glad/glad.h>

#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


char *
textFileRead(const char *fn)
{
        FILE *fichero;
        char *contenido = NULL;

        int count = 0;

        if (fn != NULL) {
                fichero = fopen(fn, "rt");

                if (fichero != NULL) {
                        fseek(fichero, 0, SEEK_END);
                        count = ftell(fichero);
                        rewind(fichero);

                        if (count > 0) {
                                contenido = (char *) malloc(sizeof(char) * (count + 1));
                                count = fread(contenido, sizeof(char), count, fichero);
                                contenido[count] = '\0';
                        }
                        fclose(fichero);
                } else {
                        std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ"
                                  << std::endl;
                        return NULL;
                }
        }

        return contenido;
}

// Nos indica e imprime por pantalla si hay algun error al crear el shader o el program

void
printShaderInfoLog(GLuint obj)
{
        int infologLength = 0;
        int charsWritten = 0;
        char *infoLog;

        glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &infologLength);

        if (infologLength > 0) {
                infoLog = (char *) malloc(infologLength);
                glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
                printf("%s\n", infoLog);
                free(infoLog);
        }
}

void
printProgramInfoLog(GLuint obj)
{
        int infologLength = 0;
        int charsWritten = 0;
        char *infoLog;

        glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &infologLength);

        if (infologLength > 0) {
                infoLog = (char *) malloc(infologLength);
                glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
                printf("%s\n", infoLog);
                free(infoLog);
        }
}


GLuint
setShaders(const char *nVertx, const char *nFrag)
{
        GLuint vertexShader, fragmentShader;
        char *ficherovs = NULL;
        char *ficherofs = NULL;
        GLuint progShader;
        const char *codigovs = NULL;
        const char *codigofs = NULL;

        vertexShader = glCreateShader(GL_VERTEX_SHADER);
        fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

        ficherovs = textFileRead(nVertx);
        if (ficherovs == NULL)
                return (GLuint) NULL;
        ficherofs = textFileRead(nFrag);
        if (ficherofs == NULL)
                return (GLuint) NULL;

        codigovs = ficherovs;
        codigofs = ficherofs;
        glShaderSource(vertexShader, 1, &codigovs, NULL);
        glShaderSource(fragmentShader, 1, &codigofs, NULL);

        // Libero los ficheros
        free(ficherovs);
        free(ficherofs);

        // Copio vertex y Fragment
        glCompileShader(vertexShader);
        glCompileShader(fragmentShader);

        // Miro si hay algun error
        printShaderInfoLog(vertexShader);
        printShaderInfoLog(fragmentShader);

        // Creo el programa asociado
        progShader = glCreateProgram();

        // Le attacheo shaders al programa
        glAttachShader(progShader, vertexShader);
        glAttachShader(progShader, fragmentShader);

        // Lo linko
        glLinkProgram(progShader);
        // A ver si hay errores
        printProgramInfoLog(progShader);
        assert(progShader > 0);

        return (progShader);
}

GLuint
setShaders_str(const char *nVertx, const char *nFrag)
{
        GLuint vertexShader, fragmentShader;
        GLuint progShader;

        // Creo el vertexShader y el FragmentShader
        vertexShader = glCreateShader(GL_VERTEX_SHADER);
        fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

        // Los cargo
        glShaderSource(vertexShader, 1, &nVertx, NULL);
        glShaderSource(fragmentShader, 1, &nFrag, NULL);

        // Copio vertex y Fragment
        glCompileShader(vertexShader);
        glCompileShader(fragmentShader);

        // Miro si hay algun error
        printShaderInfoLog(vertexShader);
        printShaderInfoLog(fragmentShader);

        // Creo el programa asociado
        progShader = glCreateProgram();

        // Le attacheo shaders al programa
        glAttachShader(progShader, vertexShader);
        glAttachShader(progShader, fragmentShader);

        // Lo linko
        glLinkProgram(progShader);
        // A ver si hay errores
        printProgramInfoLog(progShader);
        assert(progShader > 0);

        return (progShader);
}
