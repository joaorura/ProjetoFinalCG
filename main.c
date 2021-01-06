#define STB_IMAGE_IMPLEMENTATION

#include <stdbool.h>
#include <stdio.h>

#include <GL/glut.h>

#include <stb_image.h>

#include <assimp/cimport.h>        // Plain-C interface
#include <assimp/scene.h>          // Output data structure
#include <assimp/postprocess.h>    // Post processing flags

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

const float VELO = 0.2;

struct image_texture {
    bool contains;
    unsigned int id;
};

struct material {
    float diffuse[3];
    float ambient[3];
    float emissive[3];
    float specular[3];
    float shininess[1];

    struct image_texture image_diffuse;
};

struct scene {
    unsigned int size;
    float** vertexMatrix;
    float** normalMatrix;
    float** uvMatrix;
    struct material* materialArray;

    unsigned int* numVerticesArray;
};


struct camera {
    float angulo;
    float fracao;
    float x, y, z;
    float dx, dy, dz;
};

struct scene* model;

struct camera cam = {0, 0.5, -2.5, -2.5, -2.5, -2.5, 1.5, 2.};

const float fov_y = 60.;

float* build_array(unsigned int len) {
    return (float*) malloc(sizeof(float) * len);
}

void load_texture(unsigned int size, char* path, struct image_texture *img_text) {
    if (size == 0) {
        img_text->contains = false;
        return false;
    }

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        stbi_image_free(data);
    }

    img_text->contains = true;
    img_text->id = textureID;

    return true;
}


struct material* loadMaterial(struct aiMaterial* mat) {
    struct material* material = (struct material*) malloc(sizeof(struct material));
    struct aiColor4D color;
    struct aiString string;
    struct image_texture img_text;
    unsigned int size = aiGetMaterialTextureCount(mat, aiTextureType_DIFFUSE);

    aiGetMaterialTexture(mat, aiTextureType_DIFFUSE, 0, &string, NULL, NULL, NULL, NULL, NULL, NULL);
       
    aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &color);
    material->diffuse[0] = color.r;
    material->diffuse[1] = color.b;
    material->diffuse[2] = color.g;

    aiGetMaterialColor(mat, AI_MATKEY_COLOR_AMBIENT, &color);
    material->ambient[0] = color.r;
    material->ambient[1] = color.b;
    material->ambient[2] = color.g;
    
    aiGetMaterialColor(mat, AI_MATKEY_COLOR_SPECULAR, &color);
    material->specular[0] = color.r;
    material->specular[1] = color.b;
    material->specular[2] = color.g;

    aiGetMaterialColor(mat, AI_MATKEY_SHININESS, &color);
    material->shininess[0] = color.r;

    return material;
}

struct scene* getScene(const struct aiScene* sc) {
    unsigned int numVerts, numMeshes = sc->mNumMeshes;
    unsigned int* numVertsArray = (unsigned int*) malloc(sizeof(unsigned int) * numMeshes);

    struct scene* scene = (struct scene*) malloc(sizeof(struct scene));
    scene->size = numMeshes;

    struct aiMesh* mesh;
    
    struct aiFace face;
   
    
    float *vertexArray, *normalArray, *uvArray;
    
    float** vertexMatrix = (float**) malloc(sizeof(float*) * numMeshes);
    float** normalMatrix = (float**) malloc(sizeof(float*) * numMeshes);
    float** uvMatrix = (float**) malloc(sizeof(float*) * numMeshes);
    struct material** materialArray = (struct material**) malloc(sizeof(struct material*) * numMeshes);

    for (unsigned int n = 0; n < numMeshes; n++)
    {
        mesh = sc->mMeshes[n];
        unsigned int index_material = mesh->mMaterialIndex;
        struct aiMaterial* material = sc->mMaterials[index_material];
        materialArray[n] = loadMaterial(material);

        numVerts = mesh->mNumFaces * 3;

        vertexArray = (float*) malloc(sizeof(float) * numVerts * 3);
        normalArray = (float*) malloc(sizeof(float) * numVerts * 3);
        uvArray = (float*) malloc(sizeof(float) * numVerts * 2);
        
        unsigned int a = 0, b = 0, c = 0;

        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            face = mesh->mFaces[i];
            
            for (int j = 0; j < 3; j++)
            {
                struct aiVector3D uv = mesh->mTextureCoords[0][face.mIndices[j]];
                uvArray[a] = uv.x;
                uvArray[a + 1] = uv.y;
                a += 2;

                struct aiVector3D normal = mesh->mNormals[face.mIndices[j]];
                normalArray[b] = normal.x;
                normalArray[b + 1] = normal.y;
                normalArray[b + 2] = normal.z;

                b += 3;

                struct aiVector3D pos = mesh->mVertices[face.mIndices[j]];
                vertexArray[c] = pos.x;
                vertexArray[c + 1] = pos.y;
                vertexArray[c + 2] = pos.z;

                c += 3;
            }
        }

        uvMatrix[n] = uvArray;
        normalMatrix[n] = normalArray;
        vertexMatrix[n] = vertexArray;

        numVertsArray[n] = numVerts;
    }

    scene->uvMatrix = uvMatrix;
    scene->normalMatrix = normalMatrix;
    scene->vertexMatrix = vertexMatrix;
    scene->numVerticesArray = numVertsArray;
    scene->materialArray = materialArray;

    return scene;
}

struct scene* import(const char* pFile) {
    const struct aiScene* scene = aiImportFile(pFile,
        aiProcessPreset_TargetRealtime_Fast);
    
    struct scene* theScene = getScene(scene);
    aiReleaseImport(scene);

    return theScene;
}


void init(void) {
    glClearColor(0.0, 0.0, 0.0, 0.0);
}

void drawScene(struct scene* theScene) {
    float* vertexArray, * normalArray, * uvArray;
    struct material* material;
    unsigned int numVerts;


    for (unsigned int i = 0; i < theScene->size; i++) {
        vertexArray = theScene->vertexMatrix[i];
        normalArray = theScene->normalMatrix[i];
        uvArray = theScene->uvMatrix[i];
        numVerts = theScene->numVerticesArray[i];
        material = theScene->materialArray;

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);


        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, material->ambient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, material->diffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, material->specular);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, material->shininess);


        glVertexPointer(3, GL_FLOAT, 0, vertexArray);
        glNormalPointer(GL_FLOAT, 0, normalArray);
        
        glTexCoordPointer(2, GL_FLOAT, 0, uvArray);

        glDrawArrays(GL_TRIANGLES, 0, numVerts);        
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY); 

    }
}

void display(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gluLookAt(cam.x, cam.y, cam.z, cam.x + cam.dx, cam.dy, cam.z + cam.dz, 0.0f, 1.0f, 0.0f);

    glPushMatrix();
    drawScene(model);
    glPopMatrix();

    glutSwapBuffers();
}


void reshape(int w, int h) {
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(fov_y, 1.0f * WINDOW_WIDTH / WINDOW_HEIGHT, 1., 20.);
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 'a':
            cam.angulo -= 0.05f;
            cam.dx = sin(cam.angulo);
            cam.dz = -cos(cam.angulo);
            break;
        case 'd':
            cam.angulo += 0.05f;
            cam.dx = sin(cam.angulo);
            cam.dz = -cos(cam.angulo);
            break;
        case 'w':
            cam.x += cam.dx * cam.fracao;
            cam.z += cam.dz * cam.fracao;
            break;
        case 's':
            cam.x -= cam.dx * cam.fracao;
            cam.z -= cam.dz * cam.fracao;
            break;
        default:
            break;
    }
}

void readData() {
    model = import("C:\\Users\\jmess\\Documents\\CG\\Export OBJ\\cg.obj");
}

int main(int argc, char** argv) {
    readData();

    glutInit(&argc, argv);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    glClearColor(0.0, 0.0, 0.0, 1.0);

    glEnable(GL_TEXTURE_2D);

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(500, 500);
    glutCreateWindow("Quarto");

    glutDisplayFunc(display);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(fov_y, 1.0f * WINDOW_WIDTH / WINDOW_HEIGHT, 1., 20.);

    init();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMainLoop();

    return 0;

}