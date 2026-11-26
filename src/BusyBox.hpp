#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace geode::prelude;
namespace fs = std::filesystem;

struct ShellContext {
    fs::path rootDir;
    fs::path& cwd;
    std::vector<std::string>& history;
    std::map<std::string, std::string>& env;
    std::function<void(std::string const&)> out;
    std::function<void()> clearScreen;
};

class BusyBox {
public:
    static fs::path resolvePath(ShellContext& ctx, std::string const& p) {
        if (p.empty() || p == "~") return ctx.rootDir / "home/player";
        fs::path target = (p == '/') ? (ctx.rootDir / p.substr(1)) : (ctx.cwd / p);
        target = fs::weakly_canonical(target);
        // Prevent escaping virtual rootfs
        if (target.string().find(fs::weakly_canonical(ctx.rootDir).string()) != 0) {
            return ctx.rootDir;
        }
        return target;
    }

    static bool run(std::vector<std::string> args, ShellContext& ctx) {
        if (args.empty()) return true;

        // Support 'busybox <applet> ...'
        if (args == "busybox") {
            if (args.size() == 1) {
                ctx.out("BusyBox v1.36.1 (GeodeOS multi-call binary)");
                ctx.out("Applets: cat, clear, cp, date, echo, env, export, false, grep,");
                ctx.out("  head, history, hostname, ls, mkdir, mv, pwd, rm, rmdir,");
                ctx.out("  seq, sort, stat, tail, touch, true, uname, uniq, wc, which, whoami");
                return true;
            }
            args.erase(args.begin());
        }

        std::string cmd = args;

        if (cmd == "clear") { ctx.clearScreen(); return true; }
        if (cmd == "true") return true;
        if (cmd == "false") return true;
        if (cmd == "whoami") { ctx.out("player"); return true; }
        if (cmd == "hostname") { ctx.out("geodeos"); return true; }
        if (cmd == "uname") {
            if (args.size() > 1 && args == "-a")
                ctx.out("Linux geodeos 6.6.0-geode #1 SMP " + std::string(GEODE_PLATFORM_NAME));
            else
                ctx.out("Linux");
            return true;
        }
        if (cmd == "date") {
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::string dt = std::ctime(&now);
            if (!dt.empty() && dt.back() == '\n') dt.pop_back();
            ctx.out(dt);
            return true;
        }
        if (cmd == "pwd") {
            auto rel = fs::relative(ctx.cwd, ctx.rootDir).string();
            ctx.out(rel == "." ? "/" : "/" + rel);
            return true;
        }
        if (cmd == "echo") {
            std::string line;
            for (size_t i = 1; i < args.size(); ++i) {
                if (i > 1) line += " ";
                if (!args[i].empty() && args[i] == '$') {
                    auto key = args[i].substr(1);
                    line += ctx.env.count(key) ? ctx.env[key] : "";
                } else {
                    line += args[i];
                }
            }
            ctx.out(line);
            return true;
        }
        if (cmd == "env") {
            for (auto const& [k, v] : ctx.env) ctx.out(k + "=" + v);
            return true;
        }
        if (cmd == "export" && args.size() >= 2) {
            auto pos = args.find('=');
            if (pos != std::string::npos) {
                ctx.env[args.substr(0, pos)] = args.substr(pos + 1);
            }
            return true;
        }
        if (cmd == "history") {
            for (size_t i = 0; i < ctx.history.size(); ++i) {
                ctx.out(" " + std::to_string(i + 1) + "  " + ctx.history[i]);
            }
            return true;
        }
        if (cmd == "which" && args.size() >= 2) {
            ctx.out("/bin/" + args);
            return true;
        }
        if (cmd == "seq" && args.size() >= 2) {
            int end = std::stoi(args.back());
            int start = (args.size() >= 3) ? std::stoi(args) : 1;
            for (int i = start; i <= end && i <= start + 50; ++i) ctx.out(std::to_string(i));
            return true;
        }
        if (cmd == "ls") {
            fs::path target = (args.size() >= 2) ? resolvePath(ctx, args) : ctx.cwd;
            if (!fs::exists(target)) { ctx.out("ls: cannot access: No such file or directory"); return true; }
            std::string row;
            for (auto const& entry : fs::directory_iterator(target)) {
                std::string name = entry.path().filename().string();
                if (entry.is_directory()) name += "/";
                row += name + "  ";
            }
            if (!row.empty()) ctx.out(row);
            return true;
        }
        if (cmd == "mkdir" && args.size() >= 2) {
            fs::create_directories(resolvePath(ctx, args.back()));
            return true;
        }
        if (cmd == "rmdir" && args.size() >= 2) {
            fs::remove(resolvePath(ctx, args));
            return true;
        }
        if (cmd == "touch" && args.size() >= 2) {
            auto p = resolvePath(ctx, args);
            fs::create_directories(p.parent_path());
            std::ofstream(p, std::ios::app);
            return true;
        }
        if (cmd == "rm" && args.size() >= 2) {
            auto p = resolvePath(ctx, args.back());
            if (fs::exists(p)) fs::remove_all(p);
            else ctx.out("rm: cannot remove: No such file or directory");
            return true;
        }
        if (cmd == "cp" && args.size() >= 3) {
            auto src = resolvePath(ctx, args);
            auto dst = resolvePath(ctx, args);
            if (fs::exists(src)) fs::copy(src, dst, fs::copy_options::overwrite_existing | fs::copy_options::recursive);
            else ctx.out("cp: cannot stat: No such file or directory");
            return true;
        }
        if (cmd == "mv" && args.size() >= 3) {
            auto src = resolvePath(ctx, args);
            auto dst = resolvePath(ctx, args);
            if (fs::exists(src)) fs::rename(src, dst);
            else ctx.out("mv: cannot stat: No such file or directory");
            return true;
        }
        if (cmd == "stat" && args.size() >= 2) {
            auto p = resolvePath(ctx, args);
            if (!fs::exists(p)) { ctx.out("stat: No such file"); return true; }
            ctx.out("  File: " + p.filename().string());
            ctx.out("  Type: " + std::string(fs::is_directory(p) ? "directory" : "regular file"));
            if (!fs::is_directory(p)) ctx.out("  Size: " + std::to_string(fs::file_size(p)) + " bytes");
            return true;
        }
        if (cmd == "cat" && args.size() >= 2) {
            auto p = resolvePath(ctx, args);
            if (!fs::exists(p) || fs::is_directory(p)) { ctx.out("cat: " + args + ": No such file"); return true; }
            std::ifstream in(p);
            std::string line;
            while (std::getline(in, line)) ctx.out(line);
            return true;
        }
        if ((cmd == "head" || cmd == "tail") && args.size() >= 2) {
            auto p = resolvePath(ctx, args.back());
            if (!fs::exists(p) || fs::is_directory(p)) { ctx.out(cmd + ": No such file"); return true; }
            std::ifstream in(p);
            std::vector<std::string> lines;
            std::string line;
            while (std::getline(in, line)) lines.push_back(line);
            size_t count = std::min<size_t>(10, lines.size());
            size_t start = (cmd == "head") ? 0 : (lines.size() - count);
            for (size_t i = start; i < start + count; ++i) ctx.out(lines[i]);
            return true;
        }
        if (cmd == "wc" && args.size() >= 2) {
            auto p = resolvePath(ctx, args.back());
            if (!fs::exists(p) || fs::is_directory(p)) { ctx.out("wc: No such file"); return true; }
            std::ifstream in(p);
            size_t lines = 0, words = 0, bytes = 0;
            std::string line;
            while (std::getline(in, line)) {
                lines++;
                bytes += line.size() + 1;
                std::stringstream ws(line);
                std::string w;
                while (ws >> w) words++;
            }
            ctx.out(" " + std::to_string(lines) + "  " + std::to_string(words) + "  " + std::to_string(bytes) + " " + args.back());
            return true;
        }
        if (cmd == "grep" && args.size() >= 3) {
            std::string pattern = args;
            auto p = resolvePath(ctx, args);
            if (!fs::exists(p) || fs::is_directory(p)) { ctx.out("grep: No such file"); return true; }
            std::ifstream in(p);
            std::string line;
            while (std::getline(in, line)) {
                if (line.find(pattern) != std::string::npos) ctx.out(line);
            }
            return true;
        }
        if ((cmd == "sort" || cmd == "uniq") && args.size() >= 2) {
            auto p = resolvePath(ctx, args.back());
            if (!fs::exists(p) || fs::is_directory(p)) { ctx.out(cmd + ": No such file"); return true; }
            std::ifstream in(p);
            std::vector<std::string> lines;
            std::string line;
            while (std::getline(in, line)) lines.push_back(line);
            if (cmd == "sort") std::sort(lines.begin(), lines.end());
            if (cmd == "uniq") lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
            for (auto const& l : lines) ctx.out(l);
            return true;
        }

        return false; // Not a BusyBox applet
    }
};
EOF
