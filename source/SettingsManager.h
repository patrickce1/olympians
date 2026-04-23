// SettingsManager.h
class SettingsManager {
public:
    static SettingsManager* get();           // singleton accessor
    void init(const std::shared_ptr<cugl::AssetManager>& assets);
    void show();
    void hide();
    bool isVisible() const;
    void update(float dt);                   // called from your app update loop
    void render(const std::shared_ptr<cugl::SpriteBatch>& batch);

    // Settings values — add whatever you need
    float musicVolume = 1.0f;
    float sfxVolume   = 1.0f;
    bool screenEffects = true;
    bool haptics = true

    void save();   // write to file
    void load();   // read from file

private:
    static SettingsManager* _instance;
    std::shared_ptr<SettingsScene> _overlay;
};
