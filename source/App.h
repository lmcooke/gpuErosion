/**
  \file App.h

  The G3D 10.00 default starter app is configured for OpenGL 4.1 and
  relatively recent GPUs.
 */
#pragma once
#include <G3D/G3DAll.h>

/** \brief Application framework. */
class App : public GApp {

    Array<shared_ptr<Surface> > m_sceneGeometry;

protected:

    /** Called from onInit */
    void makeGUI();

public:
    
    App(const GApp::Settings& settings = GApp::Settings());

    virtual void onInit() override;
    virtual void onAI() override;
    virtual void onNetwork() override;
    virtual void onSimulation(RealTime rdt, SimTime sdt, SimTime idt) override;
    virtual void onPose(Array<shared_ptr<Surface> >& posed3D, Array<shared_ptr<Surface2D> >& posed2D) override;

    // You can override onGraphics if you want more control over the rendering loop.
    // virtual void onGraphics(RenderDevice* rd, Array<shared_ptr<Surface> >& surface, Array<shared_ptr<Surface2D> >& surface2D) override;

    virtual void onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface> >& surface3D) override;
    virtual void onGraphics2D(RenderDevice* rd, Array<shared_ptr<Surface2D> >& surface2D) override;

    virtual bool onEvent(const GEvent& e) override;
    virtual void onUserInput(UserInput* ui) override;
    virtual void onCleanup() override;

protected:
    shared_ptr<Texture> m_environmentMap;
    shared_ptr<Texture> m_terrainTex;
    shared_ptr<Texture> m_waterTex;
    shared_ptr<ThirdPersonManipulator> m_manipulator;
    shared_ptr<ArticulatedModel> m_model;

private:

    void makePlaneModel();
    void configureLightingArgs(Args& args);

    float m_time;

    int m_count;

    shared_ptr<Texture> m_prevSample;
    shared_ptr<Texture> m_nextSample;

    shared_ptr<Texture> m_prevFlux;
    shared_ptr<Texture> m_nextFlux;

    shared_ptr<Texture> m_prevVelocity;
    shared_ptr<Texture> m_nextVelocity;

    shared_ptr<Framebuffer> m_prevFBO;
    shared_ptr<Framebuffer> m_nextFBO;

    bool m_useWater;
    bool m_useSediment;
    bool m_useFlux;
    bool m_useVelocity;

    float m_kc;
    float m_ks;
    float m_kd;

    bool m_useRiver;
    bool m_useRain;

};
