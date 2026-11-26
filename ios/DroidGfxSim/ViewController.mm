// ViewController.mm — GLKViewController shell with EAGLContext OpenGLES 3.0 and touch gestures

#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>

#include "application/application.h"
#include "graphics/camera.h"
#include "graphics/gl3_renderer.h"
#include "graphics/gl2_renderer.h"
#include "graphics/renderer_interface.h"
#include "physics/simulation.h"
#include "common/asset_manager.hpp"

@interface DroidGfxSimViewController : GLKViewController <GLKViewDelegate> {
    EAGLContext *_context;
    Application *_application;
    IRenderer *_renderer;
    AssetManager *_assetManager;
    CGPoint _lastPanLocation;
}

- (void)setupGL;
- (void)tearDownGL;
- (void)handlePan:(UIPanGestureRecognizer *)gesture;
- (void)handlePinch:(UIPinchGestureRecognizer *)gesture;
- (void)handleTap:(UITapGestureRecognizer *)gesture;

@end

@implementation DroidGfxSimViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    if (!_context) {
        NSLog(@"Failed to initialize OpenGLES 3.0 context; falling back to 2.0");
        _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    }

    if (!_context) {
        NSLog(@"CRITICAL: Unable to create EAGLContext!");
        return;
    }

    GLKView *view = (GLKView *)self.view;
    view.context = _context;
    view.drawableDepthFormat = GLKViewDrawableDepthFormat24;
    view.drawableColorFormat = GLKViewDrawableColorFormatRGBA8888;
    self.preferredFramesPerSecond = 60;

    [self setupGL];

    // Setup Gesture Recognizers
    UIPanGestureRecognizer *panGesture = [[UIPanGestureRecognizer alloc]
        initWithTarget:self action:@selector(handlePan:)];
    [view addGestureRecognizer:panGesture];

    UIPinchGestureRecognizer *pinchGesture = [[UIPinchGestureRecognizer alloc]
        initWithTarget:self action:@selector(handlePinch:)];
    [view addGestureRecognizer:pinchGesture];

    UITapGestureRecognizer *tapGesture = [[UITapGestureRecognizer alloc]
        initWithTarget:self action:@selector(handleTap:)];
    [view addGestureRecognizer:tapGesture];
}

- (void)setupGL {
    [EAGLContext setCurrentContext:_context];

    // Initialize Asset Manager pointing to main bundle resources
    NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
    _assetManager = new AssetManager([resourcePath UTF8String]);

    // Initialize SceneGraph Renderer
    _renderer = new GL3SceneGraphRenderer(_assetManager);

    // Initialize Core Simulation Application
    _application = new Application(_assetManager);
    _application->init();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
}

- (void)tearDownGL {
    [EAGLContext setCurrentContext:_context];

    delete _application;
    _application = nullptr;

    delete _renderer;
    _renderer = nullptr;

    delete _assetManager;
    _assetManager = nullptr;

    if ([EAGLContext currentContext] == _context) {
        [EAGLContext setCurrentContext:nil];
    }
    _context = nil;
}

- (void)dealloc {
    [self tearDownGL];
}

#pragma mark - Gesture Handlers

- (void)handlePan:(UIPanGestureRecognizer *)gesture {
    CGPoint currentPoint = [gesture locationInView:self.view];
    if (gesture.state == UIGestureRecognizerStateBegan) {
        _lastPanLocation = currentPoint;
    } else if (gesture.state == UIGestureRecognizerStateChanged) {
        float dx = static_cast<float>(currentPoint.x - _lastPanLocation.x);
        float dy = static_cast<float>(currentPoint.y - _lastPanLocation.y);
        _lastPanLocation = currentPoint;

        if (_application && _application->getCamera()) {
            Camera *camera = _application->getCamera();
            camera->aim(dx * 0.005f, dy * 0.005f);
        }
    }
}

- (void)handlePinch:(UIPinchGestureRecognizer *)gesture {
    if (gesture.state == UIGestureRecognizerStateChanged) {
        float scaleDelta = static_cast<float>(gesture.scale - 1.0);
        gesture.scale = 1.0;

        if (_application && _application->getCamera()) {
            Camera *camera = _application->getCamera();
            camera->moveForward(scaleDelta * 5.0f);
        }
    }
}

- (void)handleTap:(UITapGestureRecognizer *)gesture {
    if (gesture.state == UIGestureRecognizerStateEnded) {
        // Spacebar equivalent: spawn dynamic sphere projectile in front of camera
        if (_application && _application->getSimulation() && _application->getCamera()) {
            Camera *camera = _application->getCamera();
            glm::vec3 spawnPos = camera->getPosition() + camera->getDirection() * 2.0f;
            glm::vec3 impulse = camera->getDirection() * 30.0f;
            _application->getSimulation()->spawnDynamicSphere(spawnPos, 0.5f, 10.0f, impulse);
        }
    }
}

#pragma mark - GLKView and GLKViewController Delegate Methods

- (void)update {
    if (_application) {
        float dt = static_cast<float>(self.timeSinceLastUpdate);
        _application->update(dt);
    }
}

- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect {
    if (!_application || !_renderer) return;

    glViewport(0, 0, static_cast<GLsizei>(view.drawableWidth), static_cast<GLsizei>(view.drawableHeight));
    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    _application->render(_renderer);
}

@end
