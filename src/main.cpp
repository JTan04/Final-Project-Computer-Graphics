/*
 Base code
 Currently will make 2 FBOs and textures (only uses one in base code)
 and writes out frame as a .png (Texture_output.png)
 
 Winter 2017 - ZJW (Piddington texture write)
 2017 integration with pitch and yaw camera lab (set up for texture mapping lab)
 */

#include <iostream>
#include <glad/glad.h>

#include "GLSL.h"
#include "Program.h"
#include "MatrixStack.h"
#include "Shape.h"
#include "WindowManager.h"
#include "GLTextureWriter.h"

// value_ptr for glm
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "stb_image.h"
#include "tiny_obj_loader.h"

using namespace std;
using namespace glm;

vector<vec3> positions(20);
vector<int> hit(20);
vector<float> explosion(20);
vec3 ballPos;


class Application : public EventCallbacks
{
    
public:
    
    // Public variables
    float time = 0;
    float speed = 0;
    float theta = 0;
    float phi = 0;
    float radius = 1;
    float x,y,z;
    float xs,ys,zs;
    const float PI = 3.14159;
    float shoot = 0;
    float explode = 0;
    
    WindowManager * windowManager = nullptr;
    
    // Our shader program
    std::shared_ptr<Program> prog;
    std::shared_ptr<Program> texProg;
    std::shared_ptr<Program> cubeProg;
    shared_ptr<Shape> cube;
    
    // Shape to be used (from obj file)
    shared_ptr<Shape> shape;
    shared_ptr<Shape> target;
    

    //ground plane info
    GLuint GrndBuffObj, GrndNorBuffObj, GrndTexBuffObj, GIndxBuffObj;
    int gGiboLen;
    
    // Contains vertex information for OpenGL
    GLuint VertexArrayID;
    
    // Data necessary to give our triangle to OpenGL
    GLuint VertexBufferID;
    
    //geometry for texture render
    GLuint quad_VertexArrayID;
    GLuint quad_vertexbuffer;
    
    //reference to texture FBO
    GLuint frameBuf[2];
    GLuint texBuf[2];
    GLuint depthBuf;
    
    unsigned int cubeMapTexture;
    
    bool FirstTime = true;
    bool Moving = false;
    int gMat = 0;
    
    
    float cTheta = 0;
    bool mouseDown = false;
    bool thrown = false;
    
    //for world
    vec3 gDTrans = vec3(0);
    float gDScale = 1.0;
    
    void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
    {
        if(key == GLFW_KEY_ESCAPE && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }
        else if (key == GLFW_KEY_M && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            gMat = (gMat + 1) % 4;
        }
        else if (key == GLFW_KEY_A && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            theta += 5*PI / 180;
        }
        else if (key == GLFW_KEY_D && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            theta -= 5*PI / 180;
        }
    }
    
    void scrollCallback(GLFWwindow* window, double deltaX, double deltaY)
    {
        // Set yaw based on deltaX
        theta += (float) deltaX / 10;
        
        // Cap pitch at 20 deg
        phi -= (float) deltaY / 10;
        if (phi > 0.936332)
            phi = 0.936332;
        else if (phi < -0.936332)
            phi = -0.936332;
    }
    
    void mouseCallback(GLFWwindow *window, int button, int action, int mods)
    {
        double posX, posY;
        
        if (action == GLFW_PRESS)
        {
            mouseDown = true;
            glfwGetCursorPos(window, &posX, &posY);
            Moving = true;
        }
        
        if (action == GLFW_RELEASE)
        {
            thrown = true;
            mouseDown = false;
        }
    }
    
    void resizeCallback(GLFWwindow *window, int width, int height)
    {
        glViewport(0, 0, width, height);
    }
    
    unsigned int createSky(string dir, vector<string> faces) {
        unsigned int textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
        
        int width, height, nrChannels;
        stbi_set_flip_vertically_on_load(false);
        for(GLuint i = 0; i < faces.size(); i++) {
            unsigned char *data =
            stbi_load((dir+faces[i]).c_str(), &width, &height, &nrChannels, 0);
            if (data) {
                glTexImage2D(
                             GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                             0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            } else {
                std::cout << "failed to load: " << (dir+faces[i]).c_str() << std::endl;
            }
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        
        cout << " creating cube map any errors : " << glGetError() << endl;
        return textureID;
    }
    
    void initTex(const std::string& resourceDirectory)
    {
        vector<std::string> faces {
            "drakeq_rt.tga",
            "drakeq_lf.tga",
            "drakeq_up.tga",
            "drakeq_dn.tga",
            "drakeq_ft.tga",
            "drakeq_bk.tga"
        };
        cubeMapTexture = createSky(resourceDirectory + "/cracks/",  faces);
    }
    
    void init(const std::string& resourceDirectory)
    {
        int width, height;
        glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
        GLSL::checkVersion();
        
        cTheta = 0;
        // Set background color.
        glClearColor(.12f, .34f, .56f, 1.0f);
        // Enable z-buffer test.
        glEnable(GL_DEPTH_TEST);
        
        // Initialize the GLSL program.
        prog = make_shared<Program>();
        prog->setVerbose(true);
        prog->setShaderNames(
                             resourceDirectory + "/simple_vert.glsl",
                             resourceDirectory + "/simple_frag.glsl");
        if (! prog->init())
        {
            std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
            exit(1);
        }
        prog->addUniform("P");
        prog->addUniform("MV");
        prog->addUniform("MatAmb");
        prog->addUniform("MatDif");
        prog->addUniform("view");
        prog->addAttribute("vertPos");
        prog->addAttribute("vertNor");
        prog->addAttribute("vertTex");
        
        //create two frame buffer objects to toggle between
        glGenFramebuffers(2, frameBuf);
        glGenTextures(2, texBuf);
        glGenRenderbuffers(1, &depthBuf);
        createFBO(frameBuf[0], texBuf[0]);
        
        //set up depth necessary as rendering a mesh that needs depth test
        glBindRenderbuffer(GL_RENDERBUFFER, depthBuf);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuf);
        
        //more FBO set up
        GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, DrawBuffers);
        
        //create another FBO so we can swap back and forth
        createFBO(frameBuf[1], texBuf[1]);
        //this one doesn't need depth
        
        //set up the shaders to blur the FBO just a placeholder pass thru now
        //next lab modify and possibly add other shaders to complete blur
        texProg = make_shared<Program>();
        texProg->setVerbose(true);
        texProg->setShaderNames(
                                resourceDirectory + "/pass_vert.glsl",
                                resourceDirectory + "/tex_fragH.glsl");
        if (! texProg->init())
        {
            std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
            exit(1);
        }
        texProg->addUniform("texBuf");
        texProg->addAttribute("vertPos");
        texProg->addAttribute("vertTex");
        texProg->addAttribute("vertNor");
        texProg->addUniform("dir");
        
        initTex(resourceDirectory);
        
        cubeProg = make_shared<Program>();
        cubeProg->setVerbose(true);
        cubeProg->setShaderNames(
                                 resourceDirectory + "/cube_vert.glsl",
                                 resourceDirectory + "/cube_frag.glsl");
        if (! cubeProg->init())
        {
            std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
            exit(1);
        }
        cubeProg->addUniform("P");
        cubeProg->addUniform("M");
        cubeProg->addUniform("V");
        cubeProg->addUniform("view");
        cubeProg->addAttribute("vertPos");
    }
    
    void initGeom(const std::string& resourceDirectory)
    {
        vector<tinyobj::shape_t> TOshapes;
        vector<tinyobj::material_t> objMaterials;
        
        string errStr;
        // Initialize the obj mesh VBOs etc
        shape = make_shared<Shape>();
        shape->loadMesh(resourceDirectory + "/sphere.obj");
        shape->resize();
        shape->init();
        //Initialize the geometry to render a quad to the screen
        initQuad();
        
        // Initialize the obj mesh VBOs etc
        target = make_shared<Shape>();
        target->loadMesh(resourceDirectory + "/cube.obj");
        target->resize();
        target->init();
        //Initialize the geometry to render a quad to the screen
        initQuad();
        
        
        cube =  make_shared<Shape>();
        cube->loadMesh(resourceDirectory + "/cube.obj");
        cube->resize();
        cube->init();
        
        
    }
    
    /**** geometry set up for a quad *****/
    void initQuad()
    {
        //now set up a simple quad for rendering FBO
        glGenVertexArrays(1, &quad_VertexArrayID);
        glBindVertexArray(quad_VertexArrayID);
        
        static const GLfloat g_quad_vertex_buffer_data[] =
        {
            -1.0f, -1.0f, 0.0f,
            1.0f, -1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f,
            1.0f, -1.0f, 0.0f,
            1.0f,  1.0f, 0.0f,
        };
        
        glGenBuffers(1, &quad_vertexbuffer);
        glBindBuffer(GL_ARRAY_BUFFER, quad_vertexbuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(g_quad_vertex_buffer_data), g_quad_vertex_buffer_data, GL_STATIC_DRAW);
        
        for (int i = 0; i<20; i++) {
            positions[i] = glm::vec3(rand() % 20 - 10, rand() % 20 - 10, rand() % 20 - 10);
            
        }
        
        float g_groundSize = 20;
        float g_groundY = -1.5;
        
        // A x-z plane at y = g_groundY of dim[-g_groundSize, g_groundSize]^2
        float GrndPos[] = {
            -g_groundSize, g_groundY, -g_groundSize,
            -g_groundSize, g_groundY,  g_groundSize,
            g_groundSize, g_groundY,  g_groundSize,
            g_groundSize, g_groundY, -g_groundSize
        };
        
        float GrndNorm[] = {
            0, 1, 0,
            0, 1, 0,
            0, 1, 0,
            0, 1, 0,
            0, 1, 0,
            0, 1, 0
        };
        
        float GrndTex[] = {
            0, 0, // back
            0, 1,
            1, 1,
            1, 0
        };
        
        unsigned short idx[] = {0, 1, 2, 0, 2, 3};
        
        GLuint VertexArrayID;
        //generate the VAO
        glGenVertexArrays(1, &VertexArrayID);
        glBindVertexArray(VertexArrayID);
        
        gGiboLen = 6;
        glGenBuffers(1, &GrndBuffObj);
        glBindBuffer(GL_ARRAY_BUFFER, GrndBuffObj);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GrndPos), GrndPos, GL_STATIC_DRAW);
        
        glGenBuffers(1, &GrndNorBuffObj);
        glBindBuffer(GL_ARRAY_BUFFER, GrndNorBuffObj);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GrndNorm), GrndNorm, GL_STATIC_DRAW);
        
        glGenBuffers(1, &GrndTexBuffObj);
        glBindBuffer(GL_ARRAY_BUFFER, GrndTexBuffObj);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GrndTex), GrndTex, GL_STATIC_DRAW);
        
        glGenBuffers(1, &GIndxBuffObj);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GIndxBuffObj);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    }
    
    /* Helper function to create the framebuffer object and
     associated texture to write to */
    void createFBO(GLuint& fb, GLuint& tex)
    {
        //initialize FBO
        int width, height;
        glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
        
        //set up framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        //set up texture
        glBindTexture(GL_TEXTURE_2D, tex);
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            cout << "Error setting up frame buffer - exiting" << endl;
            exit(0);
        }
    }
    
    // To complete image processing on the specificed texture
    // Right now just draws large quad to the screen that is texture mapped
    // with the prior scene image - next lab we will process
    void ProcessImage(GLuint inTex)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, inTex);
        
        // example applying of 'drawing' the FBO texture - change shaders
        texProg->bind();
        glUniform1i(texProg->getUniform("texBuf"), 0);
        glUniform2f(texProg->getUniform("dir"), -1, 0);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, quad_vertexbuffer);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *) 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDisableVertexAttribArray(0);
        texProg->unbind();
    }
    
    vec3 calculateTrajectory(vec3 initialVelocity, float gravityInY, float time){
        vec3 outDisplacement;
        
        outDisplacement.x = initialVelocity.x * time;
        outDisplacement.z = initialVelocity.z * time;
        
        float timeSquared = time * time;
        outDisplacement.y = (initialVelocity.y * time) + 0.5*(gravityInY * timeSquared);
        return outDisplacement;
    }
    
    
    bool checkCollision(vec3 ball, vec3 cube){
        float dx = ball.x - cube.x;
        float dy = ball.y - cube.y;
        float dz = ball.z - cube.z;
        float distance = sqrt(dx*dx + dy*dy + dz*dz);
        
        if(distance <= 1.3f){
            return true;
        }
        return false;
    }
    int cur;
    void render()
    {
        // Get current frame buffer size.
        int width, height;
        glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
        glViewport(0, 0, width, height);
        
        if (Moving)
        {
            //set up to render to buffer
            glBindFramebuffer(GL_FRAMEBUFFER, frameBuf[0]);
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        
        // Clear framebuffer.
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        /* Leave this code to just draw the meshes alone */
        float aspect = width/(float)height;
        
        // Setup yaw and pitch of camera for lookAt()
        vec3 eye = vec3(0, 0 ,0);
        x = radius*cos(phi)*cos(theta);
        y = radius*sin(phi);
        z = radius*cos(phi)*sin(theta);
        vec3 center = vec3(x, y, z);
        vec3 up = vec3(0, 1, 0);
        
        // Create the matrix stacks
        auto P = make_shared<MatrixStack>();
        auto MV = make_shared<MatrixStack>();
        
        // Apply perspective projection.
        P->pushMatrix();
        P->perspective(45.0f, aspect, 0.01f, 100.0f);
        
        //Draw our scene - two meshes - right now to a texture
        prog->bind();
        glUniformMatrix4fv(prog->getUniform("P"), 1, GL_FALSE, value_ptr(P->topMatrix()));
        
        // globl transforms for 'camera' (you will fix this now!)
        MV->pushMatrix();
        MV->loadIdentity();
            /* draw left mesh */
            MV->pushMatrix();
            MV->scale(vec3(0.01, 0.01, 0.01));
            MV->translate(vec3(0, 0, 0));
        
            MV->rotate(-theta, vec3(0,1,0));
            MV->translate(vec3(2, -1, 0));
           
            SetMaterial(3);
            glUniformMatrix4fv(prog->getUniform("MV"), 1, GL_FALSE,value_ptr(MV->topMatrix()) );
            glUniformMatrix4fv(prog->getUniform("view"), 1, GL_FALSE,value_ptr(lookAt(eye, center, up)));
            if(!thrown){
                shape->draw(prog);
            }
            MV->popMatrix();
        MV->popMatrix();
        
        P->popMatrix();
        
        prog->unbind();
        
        
        P->pushMatrix();
        P->perspective(45.0f, aspect, 0.01f, 100.0f);
        prog->bind();
        glUniformMatrix4fv(prog->getUniform("P"), 1, GL_FALSE, value_ptr(P->topMatrix()));
        
        // globl transforms for 'camera' (you will fix this now!)
        MV->pushMatrix();
        MV->loadIdentity();
            //draw the ball that gets shot
            MV->pushMatrix();
                MV->scale(vec3(0.01, 0.01, 0.01));
                MV->translate(vec3(0, 0, 0));
                MV->translate(vec3(0, 0, 0));
                vec3 yeet = calculateTrajectory(vec3(xs,ys,zs) * shoot, -.0018, time);
                ballPos = yeet/10.0f;
                MV->translate(yeet);
                SetMaterial(3);
                glUniformMatrix4fv(prog->getUniform("MV"), 1, GL_FALSE,value_ptr(MV->topMatrix()) );
                glUniformMatrix4fv(prog->getUniform("view"), 1, GL_FALSE,value_ptr(lookAt(eye, center, up)));
                shape->draw(prog);
            MV->popMatrix();
        MV->popMatrix();
        
        P->popMatrix();
        
        prog->unbind();
        
        
        
        P->pushMatrix();
        P->perspective(45.0f, aspect, 0.01f, 100.0f);
        prog->bind();
        glUniformMatrix4fv(prog->getUniform("P"), 1, GL_FALSE, value_ptr(P->topMatrix()));
        
        // globl transforms for 'camera' (you will fix this now!)
        MV->pushMatrix();
        MV->loadIdentity();
        //draw 20 cubes made of 8 different cubes
        for (int i = 0; i < 20; i++)
        {
            MV->pushMatrix();
            MV->translate((positions[i]/10.0f));
            MV->scale(vec3(0.05, 0.05, 0.05));
            if(checkCollision(ballPos, positions[i] + vec3(.5,.5,.5)) || hit[i] == 1){
                hit[i] = 1;
                cur = i;
                vec3 yeet = calculateTrajectory(vec3(xs, ys, zs) * explode, -.003, explosion[i]);
                explosion[i] += 0.5;
                MV->translate(yeet);
                MV->rotate(explosion[i]/20, vec3(0, 1, 0));
                MV->rotate(explosion[i]/20, vec3(0, 0, 1));
            }
            
            
            SetMaterial(i%4);
            glUniformMatrix4fv(prog->getUniform("MV"), 1, GL_FALSE,value_ptr(MV->topMatrix()) );
            glUniformMatrix4fv(prog->getUniform("view"), 1, GL_FALSE,value_ptr(lookAt(eye, center, up)));
            target->draw(prog);
            MV->popMatrix();

            MV->pushMatrix();
            MV->translate(positions[i]/10.0f);
            MV->translate(vec3(0.1, 0, 0));
            MV->scale(vec3(0.05, 0.05, 0.05));
            if(checkCollision(ballPos, positions[i]) || hit[i] == 1){
                yeet = calculateTrajectory(vec3(xs+positions[i].x/5.0f,ys,zs) * explode, -.003, explosion[i]);
                MV->translate(yeet);
                MV->rotate(-explosion[i]/20, vec3(1, 0, 0));
                MV->rotate(explosion[i]/20, vec3(0, 0, 1));
            }
            
            SetMaterial(i%4);
            glUniformMatrix4fv(prog->getUniform("MV"), 1, GL_FALSE,value_ptr(MV->topMatrix()) );
            glUniformMatrix4fv(prog->getUniform("view"), 1, GL_FALSE,value_ptr(lookAt(eye, center, up)));
            target->draw(prog);
            MV->popMatrix();
            
            MV->pushMatrix();
            MV->translate(positions[i]/10.0f);
            MV->translate(vec3(0, 0.1, 0));
            MV->scale(vec3(0.05, 0.05, 0.05));
            if(checkCollision(ballPos, positions[i]) || hit[i] == 1){
                yeet = calculateTrajectory(vec3(xs,ys+positions[i].y/5.0f,zs) * explode, -.003, explosion[i]);
                MV->translate(yeet);
                MV->rotate(explosion[i]/20, vec3(0, 1, 0));
                MV->rotate(-explosion[i]/20, vec3(1, 0, 0));
            }
            
            
            SetMaterial(i%4);
            glUniformMatrix4fv(prog->getUniform("MV"), 1, GL_FALSE,value_ptr(MV->topMatrix()) );
            glUniformMatrix4fv(prog->getUniform("view"), 1, GL_FALSE,value_ptr(lookAt(eye, center, up)));
            target->draw(prog);
            MV->popMatrix();
            
            
            MV->pushMatrix();
            MV->translate(positions[i]/10.0f);
            MV->translate(vec3(0, 0, 0.1));
            MV->scale(vec3(0.05, 0.05, 0.05));
            if(checkCollision(ballPos, positions[i]) || hit[i] == 1){
                yeet = calculateTrajectory(vec3(xs,ys,zs+positions[i].z/5.0f) * explode, -.003, explosion[i]);
                MV->translate(yeet);
                MV->rotate(-explosion[i]/20, vec3(1, 0, 0));
                MV->rotate(-explosion[i]/20, vec3(0, 1, 0));
            }
            
            
            SetMaterial(i%4);
            glUniformMatrix4fv(prog->getUniform("MV"), 1, GL_FALSE,value_ptr(MV->topMatrix()) );
            glUniformMatrix4fv(prog->getUniform("view"), 1, GL_FALSE,value_ptr(lookAt(eye, center, up)));
            target->draw(prog);
            MV->popMatrix();
            
            
            MV->pushMatrix();
            MV->translate(positions[i]/10.0f);
            MV->translate(vec3(0.1, 0, 0.1));
            MV->scale(vec3(0.05, 0.05, 0.05));
            if(checkCollision(ballPos, positions[i]) || hit[i] == 1){
                yeet = calculateTrajectory(vec3(xs+positions[i].x/5.0f,ys,zs+positions[i].z/5.0f) * explode, -.003, explosion[i]);
                MV->translate(yeet);
                MV->rotate(explosion[i]/20, vec3(0, 0, 1));
                MV->rotate(-explosion[i]/20, vec3(0, 1, 0));
            }
            
            
            SetMaterial(i%4);
            glUniformMatrix4fv(prog->getUniform("MV"), 1, GL_FALSE,value_ptr(MV->topMatrix()) );
            glUniformMatrix4fv(prog->getUniform("view"), 1, GL_FALSE,value_ptr(lookAt(eye, center, up)));
            target->draw(prog);
            MV->popMatrix();
            
            
            
            MV->pushMatrix();
            MV->translate(positions[i]/10.0f);
            MV->translate(vec3(0.1, 0.1, 0.1));
            MV->scale(vec3(0.05, 0.05, 0.05));
            if(checkCollision(ballPos, positions[i]) || hit[i] == 1){
                yeet = calculateTrajectory(vec3(xs+positions[i].x/5.0f,ys+positions[i].y/10.0f,zs+positions[i].z/5.0f) * explode, -.003, explosion[i]);
                MV->translate(yeet);
                MV->rotate(explosion[i]/20, vec3(1, 0, 0));
                MV->rotate(explosion[i]/20, vec3(0, 0, 1));
            }
            
            
            SetMaterial(i%4);
            glUniformMatrix4fv(prog->getUniform("MV"), 1, GL_FALSE,value_ptr(MV->topMatrix()) );
            glUniformMatrix4fv(prog->getUniform("view"), 1, GL_FALSE,value_ptr(lookAt(eye, center, up)));
            target->draw(prog);
            MV->popMatrix();
            
            
            
            MV->pushMatrix();
            MV->translate(positions[i]/10.0f);
            MV->translate(vec3(0, 0.1, 0.1));
            MV->scale(vec3(0.05, 0.05, 0.05));
            if(checkCollision(ballPos, positions[i]) || hit[i] == 1){
                yeet = calculateTrajectory(vec3(xs,ys+positions[i].y/5.0f,zs+positions[i].z/5.0f) * explode, -.003, explosion[i]);
                MV->translate(yeet);
                MV->rotate(-explosion[i]/20, vec3(0, 0, 1));
                MV->rotate(-explosion[i]/20, vec3(1, 0, 0));
            }
            
            SetMaterial(i%4);
            glUniformMatrix4fv(prog->getUniform("MV"), 1, GL_FALSE,value_ptr(MV->topMatrix()) );
            glUniformMatrix4fv(prog->getUniform("view"), 1, GL_FALSE,value_ptr(lookAt(eye, center, up)));
            target->draw(prog);
            MV->popMatrix();
            
            
            MV->pushMatrix();
            MV->translate(positions[i]/10.0f);
            MV->translate(vec3(0.1, 0.1, 0));
            MV->scale(vec3(0.05, 0.05, 0.05));
            if(checkCollision(ballPos, positions[i]) || hit[i] == 1){
                yeet = calculateTrajectory(vec3(xs+positions[i].x/5.0f,ys+positions[i].y/5.0f,zs) * explode, -.003, explosion[i]);
                MV->translate(yeet);
                MV->rotate(-explosion[i]/20, vec3(0, 1, 0));
                MV->rotate(explosion[i]/20, vec3(0, 0, 1));
            }
            
            
            SetMaterial(i%4);
            glUniformMatrix4fv(prog->getUniform("MV"), 1, GL_FALSE,value_ptr(MV->topMatrix()) );
            glUniformMatrix4fv(prog->getUniform("view"), 1, GL_FALSE,value_ptr(lookAt(eye, center, up)));
            target->draw(prog);
            MV->popMatrix();
        }
        MV->popMatrix();
        
        
        
        prog->unbind();
        
        cubeProg->bind();
        glUniformMatrix4fv(cubeProg->getUniform("P"), 1, GL_FALSE, value_ptr(P->topMatrix()));
        mat4 ident(1.0);
        glDepthFunc(GL_LEQUAL);
        MV->pushMatrix();
        MV->loadIdentity();
        MV->rotate(radians(theta), vec3(0, 1, 0));
        MV->translate(vec3(0, 0.0, 0));
        MV->scale(50.0);
        glUniformMatrix4fv(cubeProg->getUniform("V"), 1, GL_FALSE,value_ptr(MV->topMatrix()) );
        glUniformMatrix4fv(cubeProg->getUniform("M"), 1, GL_FALSE,value_ptr(ident));
        glUniformMatrix4fv(cubeProg->getUniform("view"), 1, GL_FALSE,value_ptr(lookAt(eye, center, up)));
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapTexture);
        cube->draw(texProg);
        glDepthFunc(GL_LESS);
        MV->popMatrix();
        cubeProg->unbind();
        
        P->popMatrix();
        
        
        if (mouseDown){
            speed += 0.001;
            explode = speed/2.0f;
            xs = x;
            ys = y;
            zs = z;
        }
        if (!mouseDown && speed > 0.0)
        {
            shoot = speed;
            if(time > 500){
                thrown = false;
                Moving = false;
                time=0;
                shoot = 0;
                speed = 0;
                explode = 0;
                hit[cur] = 0;
                explosion[cur] = 0;
            }else{
                time++;
            }
            for (int i = 0; i < 3; i ++)
            {
                //set up framebuffer
                glBindFramebuffer(GL_FRAMEBUFFER, frameBuf[(i+1)%2]);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                //set up texture
                ProcessImage(texBuf[i%2]);
            }
            
            /* now draw the actual output */
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            ProcessImage(texBuf[1]);
            
        }
        
    }
    
    // helper function to set materials for shading
    void SetMaterial(int i)
    {
        switch (i)
        {
            case 0: //shiny blue plastic
                glUniform3f(prog->getUniform("MatAmb"), 0.02f, 0.04f, 0.2f);
                glUniform3f(prog->getUniform("MatDif"), 0.0f, 0.16f, 0.9f);
                break;
            case 1: // flat grey
                glUniform3f(prog->getUniform("MatAmb"), 0.13f, 0.13f, 0.14f);
                glUniform3f(prog->getUniform("MatDif"), 0.3f, 0.3f, 0.4f);
                break;
            case 2: //brass
                glUniform3f(prog->getUniform("MatAmb"), 0.3294f, 0.2235f, 0.02745f);
                glUniform3f(prog->getUniform("MatDif"), 0.7804f, 0.5686f, 0.11373f);
                break;
            case 3: //copper
                glUniform3f(prog->getUniform("MatAmb"), 0.1913f, 0.0735f, 0.0225f);
                glUniform3f(prog->getUniform("MatDif"), 0.7038f, 0.27048f, 0.0828f);
                break;
        }
    }
    
};

int main(int argc, char **argv)
{
    // Where the resources are loaded from
    std::string resourceDir = "../resources";
    
    if (argc >= 2)
    {
        resourceDir = argv[1];
    }
    
    Application *application = new Application();
    
    // Your main will always include a similar set up to establish your window
    // and GL context, etc.
    
    WindowManager *windowManager = new WindowManager();
    windowManager->init(1000, 800);
    windowManager->setEventCallbacks(application);
    application->windowManager = windowManager;
    
    // This is the code that will likely change program to program as you
    // may need to initialize or set up different data and state
    
    application->init(resourceDir);
    application->initGeom(resourceDir);
    
    // Loop until the user closes the window.
    while (! glfwWindowShouldClose(windowManager->getHandle()))
    {
        // Render scene.
        application->render();
        
        // Swap front and back buffers.
        glfwSwapBuffers(windowManager->getHandle());
        // Poll for and process events.
        glfwPollEvents();
    }
    
    // Quit program.
    windowManager->shutdown();
    return 0;
}
