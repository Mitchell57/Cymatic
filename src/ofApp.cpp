#include "ofApp.h"

    
//--------------------------------------------------------------
void ofApp::setup(){
    // app init
    ofSetVerticalSync(true);
    ofSetFrameRate(60);
    ofBackground(12);
    
    // window config (for switching between retina/normal scale
    win = (ofAppGLFWWindow*)ofGetWindowPtr();
    coordScale = win->getPixelScreenCoordScale();
    ofSetWindowShape(win->getPixelScreenCoordScale()*1024, win->getPixelScreenCoordScale()*768);
    themeName = "default-theme.json";
    if(coordScale == 2) themeName = "retina-theme.json";
    
    // init variables
    file_pos = 0;
    inputMode = true;
    
    // audio init
    bufferSize = 2048;
    sampleRate = 44100;
    int channels = 1;
    
    ofSoundStreamSettings settings;
    settings.setOutListener(this);
    settings.setInListener(this);
    settings.numOutputChannels = 2;
    settings.numInputChannels = 1;
    settings.numBuffers = 8;
    settings.bufferSize = bufferSize;
    
    soundStream.setup(settings);
    
    analysis.init(sampleRate, bufferSize, channels);
    
    player.load("filetype-test.mp3");

    
    //----------------------------------------------------------------------------------
    // gui
    //----------------------------------------------------------------------------------
    
    // main panel
    //----------------------------------------------------------------------------------
    gui.setupFlexBoxLayout();
    all = gui.addGroup("Sonic Profiler", ofJson({
        {"flex-direction", "column"},
        {"flex", 1},
        {"background-color", "#181818"},
        {"flex-wrap", "wrap"},
        {"show-header", true},
        {"position", "static"},
    }));
    
    
    // input mode / file manager
    //----------------------------------------------------------------------------------
    inputParameters.setName("Input Source");
    inputParameters.add(input0.set("Microphone", false));
    inputParameters.add(input1.set("Play File", false));
    
    inputToggles = all->addGroup(inputParameters);
    inputToggles->setExclusiveToggles(true);
    inputToggles->setConfig(ofJson({{"type", "radio"}}));
    
    fileManager = inputToggles->addGroup("File Manager");
    fileManager->setShowHeader(false);
    
    playbackControls = fileManager->addGroup("Playback",ofJson({
        {"flex-direction", "row"},
        {"flex", 0},
        {"justify-content", "center"},
        {"margin", 2},
        {"padding", 2},
        {"flex-wrap", "wrap"},
        {"show-header", false},
    }));
    playbackControls->add(filePath.set("path"), ofJson({{"text-align", "center"}, {"width", "90%"}}));
    playbackControls->add(playButton.set("Play/Pause"), ofJson({{"type", "fullsize"}, {"text-align", "center"}, {"width", "45%"}}));
    playbackControls->add(resetButton.set("Reset"), ofJson({{"type", "fullsize"}, {"text-align", "center"}, {"width", "45%"}}));
    float duration = player.getDuration();
    playbackControls->add(seekSlider.set("Seek", 0, 0, duration), ofJson({{"precision", 0}, {"width", "100%"}}));
    playbackControls->add(volumeSlider.set("Volume", 100, 0, 100), ofJson({{"precision", 0}, {"width", "100%"}}));
    playbackControls->minimize();
    
    fileManager->add(loadButton.set("Choose File"), ofJson({{"type", "fullsize"}, {"text-align", "center"}}));
    fileManager->minimize();
    
    
    // misc
    //----------------------------------------------------------------------------------
    dc.setup(&analysis, ofGetWidth(), ofGetHeight(), all);
    
    all->add(minimizeButton.set("Collapse All"), ofJson({{"type", "fullsize"}, {"text-align", "center"}}));
 
    
    // listeners
    //----------------------------------------------------------------------------------
    
    // input radio
    inputToggles->getActiveToggleIndex().addListener(this, &ofApp::setInputMode);
    inputToggles->setActiveToggle(0);
    
    // file buttons
    loadButton.addListener(this, &ofApp::loadFile);
    playButton.addListener(this, &ofApp::playFile);
    resetButton.addListener(this, &ofApp::restartFile);
    
    // playback sliders
    seekSlider.addListener(this, &ofApp::seekChanged);
    volumeSlider.addListener(this, &ofApp::volumeChanged);
    
    // minimize button
    minimizeButton.addListener(this, &ofApp::minimizePressed);
    
    // load theme
    all->loadTheme(themeName);
    
    // Call resize to update control panel width and adjust drawing boxes
    windowResized(WIN_WIDTH, WIN_HEIGHT);
    
    // signal
    ready = true;
}

//-------------------------------------------------------------------------------------
// listeners
//-------------------------------------------------------------------------------------

//--------------------------------------------------------------
// Update input source and show/hide file manager
void ofApp::setInputMode(int& index){
    switch (index) {
        case 0:
            inputMode = true;
            fileManager->minimize();
            player.setPaused(true);
            break;
        case 1:
            inputMode = false;
            fileManager->maximize();
            if(fileLoaded) playbackControls->maximize();
            break;

        default:
            break;
    }
}


//--------------------------------------------------------------
// Open system dialog and allow user to choose .wav file
void ofApp::loadFile(){
    
    // Listener fires twice when pressed (click & release)
    // So this check makes sure it only prompts once
    if(!loadPressed){
        loadPressed = true;

        // Opens system dialog asking for a file
        ofFileDialogResult result = ofSystemLoadDialog("Load file");

        if(result.bSuccess) {
            fileLoaded = false;
            playbackControls->minimize();
            string path = result.getPath();
            string name = result.getName();
                try{
                    player.unload();
                    if(player.load(path)){
                        if(name.length() < 25) filePath.set(name);
                        else{
                            std::string croppedName = name.substr(0, 24)+"...";
                            filePath.set(croppedName);
                        }
                        fileLoaded = true;
                        seekSlider.set(0);
                        seekSlider.setMax(player.getDuration());
                        playbackControls->maximize();
                    }
                    else{
                        ofSystemAlertDialog("Invalid File");
                    }
                }
                catch(...){
                    ofSystemAlertDialog("Invalid File");
                }
        }
    }
    loadPressed = false;
}

//--------------------------------------------------------------
// Toggle whether a file should be playing or not
void ofApp::playFile(){
    // Listener fires twice when pressed (click & release I think)
    // So this check makes sure it only prompts once
    if(!playPressed){
        playPressed = true;
        if(player.isPlaying()) {
            player.setPaused(true);
        }
        else {
            player.play();
        }


        shouldPlayAudio = !shouldPlayAudio;
    }

    playPressed = false;
    
}

//--------------------------------------------------------------
// Toggle whether a file should be playing or not
void ofApp::restartFile(){
    
    // Listener fires twice when pressed (click & release I think)
    // So this check makes sure it only prompts once
    if(!resetPressed){
        resetPressed = true;
        player.setPaused(true);
        player.setPosition(0);
    }

    resetPressed = false;
    
}

//--------------------------------------------------------------
// Update playback position
void ofApp::seekChanged(float &val){
    if(val != file_pos){
        float pct = val / player.getDuration();
        player.setPosition(pct);
    }
}

//--------------------------------------------------------------
// Update output volume
void ofApp::volumeChanged(float &val){
    player.setVolume(val / 100.);
}

//--------------------------------------------------------------
// Collapse main panels
void ofApp::minimizePressed(){
    dc.minimize();
    inputToggles->minimize();
}

//--------------------------------------------------------------
// Expand main panels
void ofApp::maximize(){
    dc.maximize();
    inputToggles->maximize();
}

//--------------------------------------------------------------
// Retrieves current frame then sends to analysis
void ofApp::audioIn(ofSoundBuffer& buffer) {
    if(inputMode && ready)
    {
        analysis.analyzeFrame(buffer);
    }
}

//--------------------------------------------------------------
// If playing from file, retrieves current frame and sends to analysis
void ofApp::update(){
    if(!inputMode && ready){
        soundBuffer = player.getCurrentSoundBuffer(bufferSize);
        
        file_pos = player.getPosition() * player.getDuration();

        seekSlider = file_pos;
        
        analysis.analyzeFrame(soundBuffer);
    }
    
    dc.update();
}

//--------------------------------------------------------------
void ofApp::draw(){
    // Checks to see if scale changed
    if(coordScale != win->getPixelScreenCoordScale()){
        coordScale = win->getPixelScreenCoordScale();
        switch (coordScale) {
            case 1:
                all->loadTheme("default-theme.json");
                break;
            case 2:
                all->loadTheme("retina-theme.json");
                break;
            default:
                break;
        }
    }
    
    ofPushMatrix();
    ofTranslate(controlWidth, 0);
    dc.draw();
    ofPopMatrix();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if(key == ' ') player.play();
    if(key == 'q') inputMode = !inputMode;
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){
    controlWidth = all->getWidth();
    dc.updateLayout(w - controlWidth, h);
}

//--------------------------------------------------------------
void ofApp::audioOut(ofSoundBuffer& buffer){
    
}


//--------------------------------------------------------------
void ofApp::exit(){
   
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){
    
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){
    
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
    
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){
    
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){
    
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){
    
}
