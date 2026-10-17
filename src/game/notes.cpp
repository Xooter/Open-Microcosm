#include "game/notes.hpp"

#include "game/inspect.hpp"
#include "game/world.hpp"

#include <charconv>
#include <cstdio>
#include <sstream>

namespace {

std::string escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\')
            out += "\\\\";
        else if (c == '\n')
            out += "\\n";
        else if (c != '\r')
            out += c;
    }
    return out;
}

std::string unescape(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            out += s[i + 1] == 'n' ? '\n' : s[i + 1];
            ++i;
        } else {
            out += s[i];
        }
    }
    return out;
}

template <class T> std::string num(T v) {
    char buf[32];
    const auto r = std::to_chars(buf, buf + sizeof buf, v);
    return std::string(buf, r.ptr);
}

template <class T> bool read(const std::string& s, T& v) {
    return std::from_chars(s.data(), s.data() + s.size(), v).ec == std::errc{};
}

// El nombre del ser va como una palabra: los espacios, con "_".
std::string word(const std::string& s) {
    std::string out = s.empty() ? "-" : s;
    for (char& c : out)
        if (c == ' ')
            c = '_';
    return out;
}

std::string unword(const std::string& s) {
    if (s == "-")
        return {};
    std::string out = s;
    for (char& c : out)
        if (c == '_')
            c = ' ';
    return out;
}

} // namespace

// nota=<paso> <horas> <ancla> <x> <y> <tipo> <ref> <ser> <texto hasta el final de la línea>
std::string serializeNotes(const std::vector<Note>& notes) {
    std::string out;
    for (const Note& n : notes) {
        out += "nota=" + num(n.step) + ' ' + num(n.hours) + ' ' + num(static_cast<int>(n.anchor)) + ' ' + num(n.pos.x) +
               ' ' + num(n.pos.y) + ' ' + num(n.pickedKind) + ' ' + num(n.ref) + ' ' + word(n.being) + ' ' +
               escape(n.text) + '\n';
    }
    return out;
}

std::vector<Note> parseNotes(const std::string& text) {
    std::vector<Note> notes;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("nota=", 0) != 0)
            continue;
        std::istringstream fields(line.substr(5));
        std::string tok[8];
        bool ok = true;
        for (std::string& t : tok)
            ok = ok && static_cast<bool>(fields >> t);
        if (!ok)
            continue;
        Note n;
        int anchor = 0;
        ok = read(tok[0], n.step) && read(tok[1], n.hours) && read(tok[2], anchor) && read(tok[3], n.pos.x) &&
             read(tok[4], n.pos.y) && read(tok[5], n.pickedKind) && read(tok[6], n.ref);
        if (!ok || anchor < 0 || anchor > static_cast<int>(NoteAnchor::Being))
            continue;
        n.anchor = static_cast<NoteAnchor>(anchor);
        n.being = unword(tok[7]);
        std::string rest;
        std::getline(fields, rest);
        if (!rest.empty() && rest.front() == ' ')
            rest.erase(0, 1);
        n.text = unescape(rest);
        notes.push_back(std::move(n));
    }
    return notes;
}

bool notePosition(const World& world, const Note& note, Vec2& pos) {
    pos = note.pos;
    if (note.anchor == NoteAnchor::Instant)
        return false;
    if (note.anchor == NoteAnchor::Place)
        return true;
    Picked p;
    p.kind = static_cast<Picked::Kind>(note.pickedKind);
    if (p.kind == Picked::Kind::Cell || p.kind == Picked::Kind::Rock)
        p.entity = static_cast<entt::entity>(note.ref);
    else
        p.id = note.ref;
    p.at = note.pos;
    Vec2 at;
    if (!locatePicked(world, p, at))
        return false;
    pos = at;
    return true;
}

std::string notesMarkdown(const std::vector<Note>& notes) {
    std::string md;
    for (const Note& n : notes) {
        char head[160];
        if (n.anchor == NoteAnchor::Instant)
            std::snprintf(head, sizeof head, "- **t = %.2f h** (paso %llu)", n.hours, static_cast<unsigned long long>(n.step));
        else
            std::snprintf(head, sizeof head, "- **t = %.2f h** (paso %llu), %s en (%.0f, %.0f) µm", n.hours,
                          static_cast<unsigned long long>(n.step), n.being.empty() ? "lugar" : n.being.c_str(),
                          static_cast<double>(n.pos.x), static_cast<double>(n.pos.y));
        md += head;
        std::string text = n.text;
        for (std::size_t at = text.find('\n'); at != std::string::npos; at = text.find('\n', at + 3))
            text.replace(at, 1, "\n  ");
        md += ": " + text + "\n";
    }
    return md;
}
