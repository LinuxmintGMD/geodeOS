#pragma once
#include <Geode/Geode.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace geode::prelude;
namespace fs = std::filesystem;

class NanoEditorPopup : public geode::Popup<fs::path, std::function<void()>> {
protected:
    fs::path m_filePath;
    std::function<void()> m_onSave;
    CCTextInputNode* m_editorInput = nullptr;

    bool setup(fs::path filePath, std::function<void()> onSave) override {
        m_filePath = filePath;
        m_onSave = onSave;
        this->setTitle("GNU nano 7.2 - " + filePath.filename().string());

        auto winSize = m_mainLayer->getContentSize();
        auto bg = CCLayerColor::create({15, 18, 24, 245}, winSize.width - 30.f, winSize.height - 75.f);
        bg->setPosition({15.f, 42.f});
        m_mainLayer->addChild(bg);

        std::string content;
        if (fs::exists(m_filePath) && !fs::is_directory(m_filePath)) {
            std::ifstream in(m_filePath);
            std::stringstream buffer;
            buffer << in.rdbuf();
            content = buffer.str();
        }

        m_editorInput = CCTextInputNode::create(winSize.width - 44.f, winSize.height - 85.f, "Type file contents...", "chatFont.fnt");
        m_editorInput->setPosition({winSize.width / 2.f, winSize.height / 2.f + 5.f});
        m_editorInput->setMaxLabelScale(0.55f);
        m_editorInput->setString(content);
        m_mainLayer->addChild(m_editorInput);

        auto saveSpr = ButtonSprite::create("^O Save", "goldFont.fnt", "GJ_button_01.png", 0.55f);
        auto saveBtn = CCMenuItemSpriteExtra::create(saveSpr, this, menu_selector(NanoEditorPopup::onSaveFile));
        saveBtn->setPosition({winSize.width / 2.f, 24.f});
        m_buttonMenu->addChild(saveBtn);

        return true;
    }

    void onSaveFile(CCObject*) {
        fs::create_directories(m_filePath.parent_path());
        std::ofstream out(m_filePath);
        out << m_editorInput->getString();
        out.close();
        if (m_onSave) m_onSave();
        this->onClose(nullptr);
    }

public:
    static NanoEditorPopup* create(fs::path const& path, std::function<void()> onSave = nullptr) {
        auto ret = new NanoEditorPopup();
        if (ret && ret->initAnchored(400.f, 250.f, path, onSave)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};
EOF
