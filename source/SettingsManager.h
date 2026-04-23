#include "scenes/SettingsScene.h"

class SettingsManager {
public:
    static SettingsManager* get();           // singleton accessor
    void init(const std::shared_ptr<cugl::AssetManager>& assets);
    void show();
    void hide();
    bool isVisible() const;
    void update(float dt);
    void render(const std::shared_ptr<cugl::graphics::SpriteBatch&> batch);

    // Settings values — add whatever you need
    float musicVolume = 1.0f;
    float sfxVolume   = 1.0f;
    bool screenEffects = true;
    bool haptics = true;

    void save();   // write to file
    void load();   // read from file

private:
    static SettingsManager* _instance;
    std::shared_ptr<SettingsScene> _overlay;
};
