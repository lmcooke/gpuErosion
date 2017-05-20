/** \file App.cpp */
#include "App.h"

// Tells C++ to invoke command-line main() function even on OS X and Win32.
G3D_START_AT_MAIN();

#define GRID_SIZE 4 // the width of the grid
#define GRID_FACENUM 150// how many faces are along the width of grid
#define TERRAINHEIGHT_FILE "image\\heightMap1\\heightMap1_terrain.png" // image file to use for terrain height
#define WATERHEIGHT_FILE "image\\heightMap1\\heightMap1_water.png" // image file to use for terrain height

int main(int argc, const char* argv[]) {
    {
        G3DSpecification g3dSpec;
        g3dSpec.audio = false;
        initGLG3D(g3dSpec);
    }

    GApp::Settings settings(argc, argv);

    // Change the window and other startup parameters by modifying the
    // settings class.  For example:
    settings.window.caption             = argv[0];

    // Set enable to catch more OpenGL errors
    // settings.window.debugContext     = true;

    // Some common resolutions:
    // settings.window.width            =  854; settings.window.height       = 480;
    // settings.window.width            = 1024; settings.window.height       = 768;
    settings.window.width               = 1280; settings.window.height       = 720;
    //settings.window.width             = 1920; settings.window.height       = 1080;
    // settings.window.width            = OSWindow::primaryDisplayWindowSize().x; settings.window.height = OSWindow::primaryDisplayWindowSize().y;
    settings.window.fullScreen          = false;
    settings.window.resizable           = ! settings.window.fullScreen;
    settings.window.framed              = ! settings.window.fullScreen;

    // Set to true for a significant performance boost if your app can't render at 60fps, or if
    // you *want* to render faster than the display.
    settings.window.asynchronous        = false;

    settings.hdrFramebuffer.depthGuardBandThickness = Vector2int16(64, 64);
    settings.hdrFramebuffer.colorGuardBandThickness = Vector2int16(0, 0);
    settings.dataDir                    = FileSystem::currentDirectory();
    settings.screenshotDirectory        = "../journal/";

    settings.renderer.deferredShading = true;
    settings.renderer.orderIndependentTransparency = true;

    return App(settings).run();
}


App::App(const GApp::Settings& settings) : GApp(settings) {
}


// Called before the application loop begins.  Load data here and
// not in the constructor so that common exceptions will be
// automatically caught.
void App::onInit() {
    GApp::onInit();
    setFrameDuration(1.0f / 60.0f);

    // Call setScene(shared_ptr<Scene>()) or setScene(MyScene::create()) to replace
    // the default scene here.
    
    // load texture files

    m_terrainTex = Texture::fromFile(System::findDataFile(TERRAINHEIGHT_FILE), ImageFormat::SRGB8());
    m_waterTex = Texture::fromFile(System::findDataFile(WATERHEIGHT_FILE), ImageFormat::SRGB8());

    showRenderingStats      = false;

    // make plane for erosion
    makePlaneModel();


    ArticulatedModel::Specification spec;
    spec.filename = System::findDataFile("model/plane.off");
    spec.stripMaterials = true;
    renderDevice->setSwapBuffersAutomatically(false);


    m_model = ArticulatedModel::create(spec);
    m_model->pose(m_sceneGeometry, Point3(0, -0.5f, 0));



    makeGUI();
    m_manipulator = ThirdPersonManipulator::create();
    addWidget(m_manipulator);

    m_time = 0.0;
    m_count = 0;

    // initialize all textures to bind to FBOs
    m_prevSample = Texture::createEmpty("App::previousSample", m_framebuffer->width(), m_framebuffer->height(), ImageFormat::RGBA16F());
    m_prevSample->clear();

//    m_nextSample = Texture::createEmpty("App::nextSample", m_framebuffer->width(), m_framebuffer->height(), ImageFormat::RGBA16F());
    m_nextSample = Texture::createEmpty("App::nextSample", m_framebuffer->width(), m_framebuffer->height(), ImageFormat::RGBA16F());
    m_nextSample->clear();

    m_prevFlux = Texture::createEmpty("App::prevFlux", m_framebuffer->width(), m_framebuffer->height(), ImageFormat::RGBA16F());
    m_prevFlux->clear();

    m_nextFlux = Texture::createEmpty("App::prevFlux", m_framebuffer->width(), m_framebuffer->height(), ImageFormat::RGBA16F());
    m_nextFlux->clear();

    m_prevVelocity = Texture::createEmpty("App::prevVelocity", m_framebuffer->width(), m_framebuffer->height(), ImageFormat::RGBA16F());
    m_prevVelocity->clear();

    m_nextVelocity = Texture::createEmpty("App::nextVelocity", m_framebuffer->width(), m_framebuffer->height(), ImageFormat::RGBA16F());
    m_nextVelocity->clear();

    // create FBOs
    m_prevFBO = Framebuffer::create(m_prevSample);
    m_nextFBO = Framebuffer::create(m_nextSample);


    // set color attachment points
    m_prevFBO->set(Framebuffer::AttachmentPoint::COLOR1, m_prevSample);
    m_nextFBO->set(Framebuffer::AttachmentPoint::COLOR1, m_nextSample);

    m_prevFBO->set(Framebuffer::AttachmentPoint::COLOR2, m_prevFlux);
    m_nextFBO->set(Framebuffer::AttachmentPoint::COLOR2, m_nextFlux);

    m_prevFBO->set(Framebuffer::AttachmentPoint::COLOR3, m_prevVelocity);
    m_nextFBO->set(Framebuffer::AttachmentPoint::COLOR3, m_nextVelocity);

    // initialize texture visualization settings.
    m_useWater = false;
    m_useSediment = false;
    m_useFlux = false;
    m_useVelocity = false;

    // set default values for erosion/deposition constants
    m_kc = .4;
    m_ks = .005;
    m_kd = .6;

    // set default values for water sources
    m_useRiver = true;
    m_useRain = false;

}


void App::makeGUI() {
    // Initialize the developer HUD
    createDeveloperHUD();

    debugWindow->setVisible(true);
    developerWindow->videoRecordDialog->setEnabled(true);

    GuiPane* infoPane = debugPane->addPane("Simulation Options", GuiTheme::ORNATE_PANE_STYLE);
    
    // Example of how to add debugging controls
    infoPane->addLabel("Show Additional Attributes:");


    infoPane->addCheckBox("Use Water Texture", &m_useWater);
    infoPane->addCheckBox("Use Sediment Texture", &m_useSediment);
    infoPane->addCheckBox("Use Flux Texture", &m_useFlux);
    infoPane->addCheckBox("Use Velocity Texture", &m_useVelocity);
    infoPane->pack();

    // second pane with erosion constants
    GuiPane* pane2 = debugPane->addPane("Adjust Erosion Constants", GuiTheme::ORNATE_PANE_STYLE);

    pane2->addNumberBox("Kc", &m_kc, "", GuiTheme::LOG_SLIDER, 0.0f, 1.0f)->setUnitsSize(1.0f);
    pane2->addNumberBox("Ks", &m_ks, "", GuiTheme::LOG_SLIDER, 0.0f, 1.0f)->setUnitsSize(1.0f);
    pane2->addNumberBox("Kd", &m_kd, "", GuiTheme::LOG_SLIDER, 0.0f, 1.0f)->setUnitsSize(1.0f);
    pane2->pack();
    pane2->moveRightOf(infoPane);

    // third pane with water options
    GuiPane* pane3 = debugPane->addPane("Change Water Source", GuiTheme::ORNATE_PANE_STYLE);
    pane3->addCheckBox("Use River", &m_useRiver);
    pane3->addCheckBox("Use Rain", &m_useRain);
    pane3->pack();
    pane3->moveRightOf(pane2);

    debugWindow->pack();
    debugWindow->setRect(Rect2D::xywh(0, 0, (float)window()->width(), debugWindow->rect().height()));

}


// This default implementation is a direct copy of GApp::onGraphics3D to make it easy
// for you to modify. If you aren't changing the hardware rendering strategy, you can
// delete this override entirely.
void App::onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface> >& allSurfaces) {

    m_time += 0.15;

    // do water incrementation
    rd->push2D(m_prevFBO); {
        rd->setColorClearValue(Color3::white() * 0.3f);
        rd->clear();
        rd->setBlendFunc(RenderDevice::BLEND_ONE, RenderDevice::BLEND_ONE);

        Args args;
        args.setRect(rd->viewport());
        args.setUniform("intensity", 1.0f);
        args.setUniform("bounds", m_prevFBO->vector2Bounds());
        args.setUniform("gridSize", GRID_SIZE);
        args.setUniform("gridFaceNum", GRID_FACENUM);
        args.setUniform("terrainHeight", m_terrainTex, Sampler::buffer());
        args.setUniform("waterHeight", m_waterTex, Sampler::buffer());
        args.setUniform("timeTick", m_time);

        args.setUniform("screenHeight", rd->height());
        args.setUniform("screenWidth", rd->width());

        // set water source args
        args.setUniform("useRiver", static_cast<int>(m_useRiver));
        args.setUniform("useRain", static_cast<int>(m_useRain));

        if (m_time < .2) {
            args.setUniform("prevSample", m_terrainTex, Sampler::buffer());
            args.setUniform("prevFlux", m_terrainTex, Sampler::buffer());
            args.setUniform("prevVelocity", m_terrainTex, Sampler::buffer());
        } else {
            args.setUniform("prevSample", m_nextFBO->texture(1), Sampler::buffer());
            args.setUniform("prevFlux", m_nextFBO->texture(2), Sampler::buffer());
            args.setUniform("prevVelocity", m_nextFBO->texture(3), Sampler::buffer());
        }

        LAUNCH_SHADER("waterIncrement.*", args);


    } rd->popState();

    // update outflow flux
    rd->push2D(m_nextFBO); {
        rd->setColorClearValue(Color3::white() * 0.3f);
        rd->clear();
        rd->setBlendFunc(RenderDevice::BLEND_ONE, RenderDevice::BLEND_ONE);

        Args args1;

        args1.setRect(rd->viewport());
        args1.setUniform("intensity", 0.6f);
        args1.setUniform("bounds", m_nextFBO->vector2Bounds());
        args1.setUniform("gridSize", GRID_SIZE);
        args1.setUniform("gridFaceNum", GRID_FACENUM);
        args1.setUniform("timeTick", m_time);

        args1.setUniform("screenHeight", rd->height());
        args1.setUniform("screenWidth", rd->width());

        args1.setUniform("prevSample", m_prevFBO->texture(1), Sampler::buffer());
        args1.setUniform("prevFlux", m_prevFBO->texture(2), Sampler::buffer());
        args1.setUniform("prevVelocity", m_prevFBO->texture(3), Sampler::buffer());

        LAUNCH_SHADER("outflowFlux.*", args1);

    } rd->popState();

    // update water height and velocity field
    rd->push2D(m_prevFBO); {
        rd->setColorClearValue(Color3::white() * 0.3f);
        rd->clear();
        rd->setBlendFunc(RenderDevice::BLEND_ONE, RenderDevice::BLEND_ONE);

        Args args1;

        args1.setRect(rd->viewport());
        args1.setUniform("intensity", 0.6f);
        args1.setUniform("bounds", m_prevFBO->vector2Bounds());
        args1.setUniform("gridSize", GRID_SIZE);
        args1.setUniform("gridFaceNum", GRID_FACENUM);
        args1.setUniform("timeTick", m_time);

        args1.setUniform("screenHeight", rd->height());
        args1.setUniform("screenWidth", rd->width());

        args1.setUniform("prevSample", m_nextFBO->texture(1), Sampler::buffer());
        args1.setUniform("prevFlux", m_nextFBO->texture(2), Sampler::buffer());
        args1.setUniform("prevVelocity", m_nextFBO->texture(3), Sampler::buffer());

        LAUNCH_SHADER("velocityField.*", args1);

    } rd->popState();

    rd->push2D(m_nextFBO); {
        rd->setColorClearValue(Color3::white() * 0.3f);
        rd->clear();
        rd->setBlendFunc(RenderDevice::BLEND_ONE, RenderDevice::BLEND_ONE);

        Args args1;

        args1.setRect(rd->viewport());
        args1.setUniform("intensity", 0.6f);
        args1.setUniform("bounds", m_nextFBO->vector2Bounds());
        args1.setUniform("gridSize", GRID_SIZE);
        args1.setUniform("gridFaceNum", GRID_FACENUM);
        args1.setUniform("timeTick", m_time);

        args1.setUniform("screenHeight", rd->height());
        args1.setUniform("screenWidth", rd->width());

        // set erosion/deposition constants from GUI
        args1.setUniform("kc", m_kc);
        args1.setUniform("ks", m_ks);
        args1.setUniform("kd", m_kd);

        args1.setUniform("prevSample", m_prevFBO->texture(1), Sampler::buffer());
        args1.setUniform("prevFlux", m_prevFBO->texture(2), Sampler::buffer());
        args1.setUniform("prevVelocity", m_prevFBO->texture(3), Sampler::buffer());

        LAUNCH_SHADER("erosion.*", args1);

    } rd->popState();

    rd->push2D(m_prevFBO); {
        rd->setColorClearValue(Color3::white() * 0.3f);
        rd->clear();
        rd->setBlendFunc(RenderDevice::BLEND_ONE, RenderDevice::BLEND_ONE);

        Args args1;

        args1.setRect(rd->viewport());
        args1.setUniform("intensity", 0.6f);
        args1.setUniform("bounds", m_prevFBO->vector2Bounds());
        args1.setUniform("gridSize", GRID_SIZE);
        args1.setUniform("gridFaceNum", GRID_FACENUM);
        args1.setUniform("timeTick", m_time);

        args1.setUniform("screenHeight", rd->height());
        args1.setUniform("screenWidth", rd->width());

        args1.setUniform("prevSample", m_nextFBO->texture(1), Sampler::buffer());
        args1.setUniform("prevFlux", m_nextFBO->texture(2), Sampler::buffer());
        args1.setUniform("prevVelocity", m_nextFBO->texture(3), Sampler::buffer());

        LAUNCH_SHADER("sediment.*", args1);

    } rd->popState();

    rd->push2D(m_nextFBO); {
        rd->setColorClearValue(Color3::white() * 0.3f);
        rd->clear();
        rd->setBlendFunc(RenderDevice::BLEND_ONE, RenderDevice::BLEND_ONE);

        Args args1;

        args1.setRect(rd->viewport());
        args1.setUniform("intensity", 0.6f);
        args1.setUniform("bounds", m_nextFBO->vector2Bounds());
        args1.setUniform("gridSize", GRID_SIZE);
        args1.setUniform("gridFaceNum", GRID_FACENUM);
        args1.setUniform("timeTick", m_time);

        args1.setUniform("screenHeight", rd->height());
        args1.setUniform("screenWidth", rd->width());

        args1.setUniform("prevSample", m_prevFBO->texture(1), Sampler::buffer());
        args1.setUniform("prevFlux", m_prevFBO->texture(2), Sampler::buffer());
        args1.setUniform("prevVelocity", m_prevFBO->texture(3), Sampler::buffer());

        LAUNCH_SHADER("evaporation.*", args1);

    } rd->popState();


    // push final shader to render to plane
    rd->pushState(m_framebuffer); {
        rd->setColorClearValue(Color3::white() * 0.3f);
        rd->clear();
        rd->setBlendFunc(RenderDevice::BLEND_ONE, RenderDevice::BLEND_ONE);


        Args args2;
        args2.setUniform("intensity", 0.6f);
        args2.setUniform("bounds", m_framebuffer->vector2Bounds());
        args2.setUniform("gridSize", GRID_SIZE);
        args2.setUniform("terrainHeight", m_terrainTex, Sampler::buffer());
        args2.setUniform("waterHeight", m_waterTex, Sampler::buffer());
        args2.setUniform("timeTick", m_time);

        args2.setUniform("prevSample", m_nextFBO->texture(1), Sampler::buffer());
        args2.setUniform("prevFlux", m_nextFBO->texture(2), Sampler::buffer());
        args2.setUniform("prevVelocity", m_nextFBO->texture(3), Sampler::buffer());

        // visualization settings
        args2.setUniform("useWaterColor", static_cast<int>(m_useWater));
        args2.setUniform("useSedimentColor", static_cast<int>(m_useSediment));
        args2.setUniform("useFluxColor", static_cast<int>(m_useFlux));
        args2.setUniform("useVelocityColor", static_cast<int>(m_useVelocity));

        configureLightingArgs(args2);

        CFrame cframe;

        for (int i = 0; i < m_sceneGeometry.size(); i++) {
            const shared_ptr<UniversalSurface>& surface = dynamic_pointer_cast<UniversalSurface>(m_sceneGeometry[i]);
            if (notNull(surface)) {
                surface->getCoordinateFrame(cframe);
                args2.setUniform("MVP", rd->invertYMatrix() * rd->projectionMatrix() * rd->cameraToWorldMatrix().inverse() * cframe);
                surface->gpuGeom()->setShaderArgs(args2);

                LAUNCH_SHADER("plane.*", args2);
            }
        }

    } rd->popState();

    swapBuffers();
    rd->clear();

    FilmSettings filmSettings = activeCamera()->filmSettings();
    filmSettings.setBloomStrength(0.0);
    filmSettings.setGamma(1.0); // default is 2.0

    m_film->exposeAndRender(rd, filmSettings, m_framebuffer->texture(0),
                            settings().hdrFramebuffer.colorGuardBandThickness.x + settings().hdrFramebuffer.depthGuardBandThickness.x,
                            settings().hdrFramebuffer.depthGuardBandThickness.x);

    m_count += 1;

}

// sets up parameters to send to shader for phong lighting
void App::configureLightingArgs(Args& args)
{
    Vector3 dirLight = Vector3(0.0, -1.0, 0.0);

    args.setUniform("wsEyePosition", m_debugCamera->frame().translation);

    args.setUniform("wsLight", dirLight);
    args.setUniform("lightColor", Color3(.75));
    args.setUniform("ambient", Color3(0.3));

}

// generate .off file of plane with specified grid size and resolution.
// Model file is created in image/plane.off.
void App::makePlaneModel()
{
    float meshWidth = static_cast<float>(GRID_SIZE);
    float meshHeight = static_cast<float>(GRID_SIZE);

    float faceNumWidthFl = static_cast<float>(GRID_FACENUM);
    float faceNumHeightFl = static_cast<float>(GRID_FACENUM);

    float faceWidth = meshWidth/faceNumWidthFl;
    float faceHeight = meshHeight/faceNumHeightFl;

    // write top of mesh .off file
    TextOutput * to1 = new TextOutput("model/plane.off");
    to1->writeSymbol("OFF");
    to1->writeNewline();
    to1->writeNewline();

    // write stats
    to1->printf("%d %d 0", (GRID_FACENUM + 1) * (GRID_FACENUM + 1),
                GRID_FACENUM * GRID_FACENUM);
    to1->writeNewline();
    to1->writeNewline();

    // vertex data
    for (int i = 0; i < GRID_FACENUM + 1; i++) {
        for (int j = 0; j < GRID_FACENUM + 1; j++) {
            float iFloat = static_cast<float>(i);
            float jFloat = static_cast<float>(j);

            float xCoord = -1.f * (.5 * GRID_SIZE) + (faceWidth * jFloat);
            float zCoord = (.5 * GRID_SIZE) - (faceHeight * iFloat);

            to1->writeNewline();
            to1->printf("%f %f %f", xCoord, 0.f, zCoord);
        }
    }

    to1->writeNewline();

    // face data
    for (int k = 0; k < GRID_FACENUM; k++) {
        for (int l = 0; l < GRID_FACENUM; l++) {
            int one = (k * (GRID_FACENUM + 1)) + l;
            int two = ((k+1) * (GRID_FACENUM + 1)) + l;
            int three = ((k+1) * (GRID_FACENUM + 1)) + l + 1;
            int four = (k * (GRID_FACENUM + 1)) + l + 1;
            to1->writeNewline();
            to1->printf("4 %d %d %d %d", four, three, two, one);
        }
    }
    to1->commit(true);
}


void App::onAI() {
    GApp::onAI();
    // Add non-simulation game logic and AI code here
}


void App::onNetwork() {
    GApp::onNetwork();
    // Poll net messages here
}


void App::onSimulation(RealTime rdt, SimTime sdt, SimTime idt) {
    GApp::onSimulation(rdt, sdt, idt);

    // Example GUI dynamic layout code.  Resize the debugWindow to fill
    // the screen horizontally.
    debugWindow->setRect(Rect2D::xywh(0, 0, (float)window()->width(), debugWindow->rect().height()));
}


bool App::onEvent(const GEvent& event) {
    // Handle super-class events
    if (GApp::onEvent(event)) { return true; }

    // If you need to track individual UI events, manage them here.
    // Return true if you want to prevent other parts of the system
    // from observing this specific event.
    //
    // For example,
    // if ((event.type == GEventType::GUI_ACTION) && (event.gui.control == m_button)) { ... return true; }
    // if ((event.type == GEventType::KEY_DOWN) && (event.key.keysym.sym == GKey::TAB)) { ... return true; }
    // if ((event.type == GEventType::KEY_DOWN) && (event.key.keysym.sym == 'p')) { ... return true; }

    if ((event.type == GEventType::KEY_DOWN) && (event.key.keysym.sym == 'p')) { 
        shared_ptr<DefaultRenderer> r = dynamic_pointer_cast<DefaultRenderer>(m_renderer);
        r->setDeferredShading(! r->deferredShading());
        return true; 
    }

    return false;
}


void App::onUserInput(UserInput* ui) {
    GApp::onUserInput(ui);
    (void)ui;
    // Add key handling here based on the keys currently held or
    // ones that changed in the last frame.
}


void App::onPose(Array<shared_ptr<Surface> >& surface, Array<shared_ptr<Surface2D> >& surface2D) {
    GApp::onPose(surface, surface2D);

    // Append any models to the arrays that you want to later be rendered by onGraphics()
}


void App::onGraphics2D(RenderDevice* rd, Array<shared_ptr<Surface2D> >& posed2D) {
    // Render 2D objects like Widgets.  These do not receive tone mapping or gamma correction.
    Surface2D::sortAndRender(rd, posed2D);
}


void App::onCleanup() {
    // Called after the application loop ends.  Place a majority of cleanup code
    // here instead of in the constructor so that exceptions can be caught.
}
