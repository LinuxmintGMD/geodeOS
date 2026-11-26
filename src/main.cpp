#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/utils/web.hpp>
#include "NanoEditor.hpp"
#include "BusyBox.hpp"

using namespace geode::prelude;
namespace fs = std::filesystem;

class TerminalPopup : public geode::Popup<>, public TextInputDelegate {
protected:
    SimpleTextArea* m_outputArea = nullptr;
    CCTextInputNode* m_inputNode = nullptr;
    std::string m_screenText = "GeodeOS 1.1.0-tty1 (BusyBox + geopkg + apt + dpkg)\nType 'help' or 'busybox' for commands.\n\n";
    fs::path m_rootDir;
    fs::path m_cwd;
    std::vector<std::string> m_cmdHistory;
    std::map<std::string, std::string> m_env;

    bool setup() override {
        this->setTitle("GeodeOS Terminal");

        m_rootDir = Mod::get()->getSaveDir() / "rootfs";
        fs::create_directories(m_rootDir / "var/lib/dpkg");
        fs::create_directories(m_rootDir / "etc/apt");
        fs::create_directories(m_rootDir / "home/player");
        m_cwd = m_rootDir / "home/player";

        m_env["USER"] = "player";
        m_env["HOME"] = "/home/player";
        m_env["SHELL"] = "/bin/ash";
        m_env["PATH"] = "/usr/local/bin:/usr/bin:/bin";

        auto winSize = m_mainLayer->getContentSize();

        auto bg = CCLayerColor::create({10, 14, 18, 240}, winSize.width - 30.f, winSize.height - 75.f);
        bg->setPosition({15.f, 42.f});
        m_mainLayer->addChild(bg);

        m_outputArea = SimpleTextArea::create(m_screenText, "chatFont.fnt", 0.50f, winSize.width - 44.f);
        m_outputArea->setAnchorPoint({0.f, 1.f});
        m_outputArea->setPosition({22.f, winSize.height - 40.f});
        m_outputArea->setColor({120, 255, 160});
        m_mainLayer->addChild(m_outputArea);

        auto inputBg = CCLayerColor::create({20, 28, 36, 255}, winSize.width - 30.f, 24.f);
        inputBg->setPosition({15.f, 14.f});
        m_mainLayer->addChild(inputBg);

        m_inputNode = CCTextInputNode::create(winSize.width - 90.f, 24.f, "$ enter command...", "chatFont.fnt");
        m_inputNode->setPosition({(winSize.width - 50.f) / 2.f + 15.f, 26.f});
        m_inputNode->setMaxLabelScale(0.58f);
        m_inputNode->setAllowedChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_./~$|><= ");
        m_inputNode->setDelegate(this);
        m_mainLayer->addChild(m_inputNode);

        auto runSpr = ButtonSprite::create("Run", "goldFont.fnt", "GJ_button_01.png", 0.5f);
        auto runBtn = CCMenuItemSpriteExtra::create(runSpr, this, menu_selector(TerminalPopup::onRunCommand));
        runBtn->setPosition({winSize.width - 38.f, 26.f});
        m_buttonMenu->addChild(runBtn);

        return true;
    }

    void textInputReturn(CCTextInputNode*) override {
        this->onRunCommand(nullptr);
    }

    void printLine(std::string const& line) {
        m_screenText += line + "\n";
        std::vector<std::string> lines;
        std::stringstream ss(m_screenText);
        std::string item;
        while (std::getline(ss, item, '\n')) lines.push_back(item);
        if (lines.size() > 14) {
            m_screenText.clear();
            for (size_t i = lines.size() - 14; i < lines.size(); ++i) {
                m_screenText += lines[i] + "\n";
            }
        }
        if (m_outputArea) m_outputArea->setText(m_screenText);
    }

    std::string getVirtualPath(fs::path const& target) {
        auto rel = fs::relative(target, m_rootDir).string();
        return (rel == ".") ? "/" : "/" + rel;
    }

    void onRunCommand(CCObject*) {
        std::string cmd = m_inputNode->getString();
        if (cmd.empty()) return;

        m_inputNode->setString("");
        m_cmdHistory.push_back(cmd);
        printLine("player@geodeos:" + getVirtualPath(m_cwd) + "$ " + cmd);
        executeCommand(cmd);
    }

    void executeCommand(std::string const& raw) {
        std::stringstream ss(raw);
        std::vector<std::string> tokens;
        std::string tok;
        while (ss >> tok) tokens.push_back(tok);
        if (tokens.empty()) return;

        // Check for output redirection (> or >>)
        std::string redirectFile;
        bool appendMode = false;
        for (size_t i = 0; i < tokens.size(); ++i) {
            if ((tokens[i] == ">" || tokens[i] == ">>") && i + 1 < tokens.size()) {
                appendMode = (tokens[i] == ">>");
                redirectFile = tokens[i + 1];
                tokens.erase(tokens.begin() + i, tokens.end());
                break;
            }
        }
        if (tokens.empty()) return;

        bool isSudo = false;
        if (tokens == "sudo") {
            isSudo = true;
            tokens.erase(tokens.begin());
            if (tokens.empty()) { printLine("usage: sudo <command>"); return; }
        }

        // Capture output if redirecting to a file
        std::string redirectedBuffer;
        auto outputFn = [&](std::string const& line) {
            if (!redirectFile.empty()) redirectedBuffer += line + "\n";
            else printLine(line);
        };

        ShellContext ctx{
            m_rootDir,
            m_cwd,
            m_cmdHistory,
            m_env,
            outputFn,
            [this]() { m_screenText.clear(); m_outputArea->setText(""); }
        };

        if (tokens == "help") {
            outputFn("Editor: nano <file> | vi <file>");
            outputFn("Packages: geopkg, apt, dpkg (use sudo for install)");
            outputFn("BusyBox: type 'busybox' for 30+ Linux utilities (supports > and >>)");
        } else if (tokens == "neofetch") {
            outputFn("OS: GeodeOS Desktop (Geometry Dash)");
            outputFn("Shell: BusyBox ash + GNU nano 7.2");
            outputFn("Platform: " + std::string(GEODE_PLATFORM_NAME));
            outputFn("Mods Loaded: " + std::to_string(Loader::get()->getAllMods().size()));
        } else if (tokens == "cd") {
            m_cwd = (tokens.size() < 2) ? (m_rootDir / "home/player") : BusyBox::resolvePath(ctx, tokens);
            if (!fs::exists(m_cwd) || !fs::is_directory(m_cwd)) {
                printLine("cd: No such directory");
                m_cwd = m_rootDir / "home/player";
            }
        } else if (tokens == "nano" || tokens == "vi") {
            if (tokens.size() < 2) {
                printLine("Usage: " + tokens + " <filename>");
            } else {
                auto target = BusyBox::resolvePath(ctx, tokens);
                NanoEditorPopup::create(target, [this, tokens]() {
                    printLine("[ Wrote " + tokens + " ]");
                })->show();
            }
        } else if (tokens == "geopkg") {
            handleGeopkg(tokens, isSudo);
        } else if (tokens == "apt" || tokens == "apt-get") {
            handleApt(tokens, isSudo);
        } else if (tokens == "dpkg") {
            handleDpkg(tokens, isSudo);
        } else if (!BusyBox::run(tokens, ctx)) {
            outputFn("ash: " + tokens + ": not found");
        }

        // Write redirected output to file if > or >> was used
        if (!redirectFile.empty()) {
            auto dest = BusyBox::resolvePath(ctx, redirectFile);
            fs::create_directories(dest.parent_path());
            std::ofstream out(dest, appendMode ? std::ios::app : std::ios::trunc);
            out << redirectedBuffer;
        }
    }

    void handleGeopkg(std::vector<std::string> const& args, bool isSudo) {
        if (args.size() < 2) { printLine("Usage: geopkg [list | install <mod-id>]"); return; }
        if (args == "list") {
            for (auto* mod : Loader::get()->getAllMods()) {
                printLine(" - " + mod->getID() + " (" + mod->getVersion().toString() + ")");
            }
        } else if (args == "install") {
            if (!isSudo) { printLine("E: Permission denied - try 'sudo geopkg install <mod-id>'"); return; }
            if (args.size() < 3) { printLine("E: Specify a mod ID."); return; }
            installGeodeMod(args);
        }
    }

    void installGeodeMod(std::string const& modId) {
        printLine("Querying Geode Index for " + modId + "...");
        Ref<TerminalPopup> self = this;

        web::WebRequest().get("https://geode-sdk.org" + modId).listen(
            [self, modId](web::WebResponse* res) {
                if (!res->ok()) { self->printLine("E: Unable to locate mod '" + modId + "'"); return; }
                auto json = res->json().unwrapOr(matjson::Value());
                auto versions = json["payload"]["versions"];
                if (!versions.isArray() || versions.asArray().unwrap().empty()) {
                    self->printLine("E: No versions found for " + modId);
                    return;
                }
                auto firstVer = versions.asArray().unwrap();
                std::string url = firstVer["download_link"].asString().unwrapOr("");
                std::string ver = firstVer["version"].asString().unwrapOr("latest");
                self->printLine("Downloading " + modId + " v" + ver + "...");

                auto dest = dirs::getModsDir() / (modId + ".geode");
                web::WebRequest().get(url).listen(
                    [self, modId, dest](web::WebResponse* fileRes) {
                        if (fileRes->ok() && fileRes->into(dest).isOk())
                            self->printLine("Installed " + modId + "! Restart GD to load.");
                        else
                            self->printLine("E: Failed to write .geode file.");
                    }
                );
            }
        );
    }

    void handleApt(std::vector<std::string> const& args, bool isSudo) {
        if (args.size() < 2) { printLine("Usage: apt [update | install <pkg>]"); return; }
        if (args == "update") {
            if (!isSudo) { printLine("E: Could not open lock file (use sudo)"); return; }
            printLine("Hit:1 https://geode-sdk.org geode-stable InRelease");
printLine("Reading package lists... Done");
} else if (args == "install") {
if (!isSudo) { printLine("E: Could not open lock file (use sudo)"); return; }
if (args.size() < 3) { printLine("E: Unable to locate package"); return; }
printLine("Setting up " + args + " (1.0.0) ...");
std::ofstream out(m_rootDir / "var/lib/dpkg/status", std::ios::app);
out << "ii  " << args << "  1.0.0  Installed via apt\n";
}
}
void handleDpkg(std::vectorstd::string const& args, bool isSudo) {
if (args.size() < 2) { printLine("dpkg: error: need an action option (-l, -i)"); return; }
auto statusFile = m_rootDir / "var/lib/dpkg/status";
if (args == "-l" || args == "--list") {
printLine("ii  geode-core   " + Loader::get()->getVersion().toString() + "   Geode Mod Loader");
printLine("ii  busybox      1.36.1   Multi-call Linux utilities");
printLine("ii  nano         7.2      Small friendly text editor");
printLine("ii  geopkg       1.0.0    GD Mod Package Manager");
if (fs::exists(statusFile)) {
std::ifstream in(statusFile);
std::string line;
while (std::getline(in, line)) printLine(line);
}
} else if (args == "-i" || args == "--install") {
if (!isSudo) { printLine("dpkg: error: requires superuser privilege"); return; }
if (args.size() < 3) { printLine("dpkg: error: --install needs a .deb file"); return; }
auto debPath = m_cwd / args;
if (!fs::exists(debPath)) { printLine("dpkg: error: cannot access '" + args + "'"); return; }
printLine("Unpacking " + args + " ...");
std::ofstream out(statusFile, std::ios::app);
out << "ii  " << args << "  1.0.0  Installed via dpkg\n";
printLine("Setting up " + args + " ...");
}
}
public:
static TerminalPopup* create() {
auto ret = new TerminalPopup();
if (ret && ret->initAnchored(420.f, 260.f)) {
ret->autorelease();
return ret;
}
CC_SAFE_DELETE(ret);
return nullptr;
}
};
class $modify(MyMenuLayer, MenuLayer) {
bool init() {
if (!MenuLayer::init()) return false;
auto btnSpr = ButtonSprite::create(">_", "goldFont.fnt", "GJ_button_04.png", 0.7f);
auto termBtn = CCMenuItemSpriteExtra::create(
btnSpr, this, menu_selector(MyMenuLayer::onOpenTerminal)
);
if (auto menu = this->getChildByID("bottom-menu")) {
menu->addChild(termBtn);
menu->updateLayout();
}
return true;
}
void onOpenTerminal(CCObject*) {
TerminalPopup::create()->show();
}
};
