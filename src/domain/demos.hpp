/* domain/demos.hpp — the demo databases of the kinds release
 * (okf/concepts/foundation/kinds.md, okf/concepts/platform/workspaces.md).
 *
 * The author, 2026-10-06: "again, our goal is NOT these demo applications.
 * these demo applications will be a result of our new transformation beyond
 * the current systems we have in place." So each one is made ONLY of what any
 * database can make for itself: its kinds are `kind` runes in its own `kinds`
 * mantle (not C++), its canvases are canvas runes, its categories are the tags
 * its regions give. Nothing here is a feature for one demo.
 *
 *   - the Whiskerwood campaign: D&D 5e, a party of cats of 5e races
 *   - Whisker Mart: a small cat grocery, the beginning of a point of sale
 *   - the House of Cats: a home's floor plan, smart devices and inventory
 *
 * (The Cat Colony, the outreach demo, stays as it was: seed.hpp.)
 *
 * A demo is TWO PHASES of dispatcher commands: its kinds, then its data. The
 * app registers the kinds in between (kinds::apply), because Void Core will not
 * make a rune of a glyph it has not been told about, exactly as a person makes
 * a kind in the Kinds window before making one of it. Everything is synthetic:
 * no real store, home, player or person. Monsters and races are named from the
 * D&D 5e System Reference Document and the 5e books; the numbers are game
 * statistics, the words are ours. */
#pragma once

#include "json.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace hormiga::demos {

struct Transcript {
    std::vector<std::string> kinds; // run in the `kinds` mantle, then registered
    std::vector<std::string> data;  // run in the data mantle
};

/* A quoted dispatcher argument (Void Core SPEC 6.1: double quotes, backslash
 * escapes). The demos' own words never end in a backslash. */
inline std::string q(const std::string& s) {
    std::string o = "\"";
    for (char ch : s) {
        if (ch == '"' || ch == '\\') o += '\\';
        o += ch;
    }
    return o + "\"";
}
inline std::string geo(double y, double x) {
    char b[48];
    std::snprintf(b, sizeof b, "%g,%g", y, x);
    return b;
}

struct F {
    const char* key;
    const char* label;
    const char* editor; // "" text, "date", "multiline:60"
};
struct KindDef {
    const char* glyph;
    const char* title;
    const char* plural;
    const char* icon;
    const char* color;
    const char* category;
    std::vector<F> fields;
    bool located, dated, listed;
    const char* date_field;
    const char* subtitle_field;
};

inline void kind(std::vector<std::string>& c, const KindDef& k) {
    const std::string g = k.glyph;
    nlohmann::json fs = nlohmann::json::array();
    for (const auto& f : k.fields) fs.push_back({{"key", f.key}, {"label", f.label}, {"editor", f.editor}});
    c.push_back("rune new kind " + g);
    c.push_back("set " + g + " title " + q(k.title));
    c.push_back("set " + g + " plural " + q(k.plural));
    c.push_back("set " + g + " icon " + q(k.icon));
    c.push_back("set " + g + " color " + q(k.color));
    c.push_back("set " + g + " category " + q(k.category));
    c.push_back("setjson " + g + " fields '" + fs.dump() + "'"); // no apostrophes in the demos' labels
    if (*k.date_field) c.push_back("set " + g + " date_field " + q(k.date_field));
    if (*k.subtitle_field) c.push_back("set " + g + " subtitle_field " + q(k.subtitle_field));
    std::string tags = " +type:kind";
    if (k.located) tags += " +trait:located";
    if (k.dated) tags += " +trait:dated";
    if (k.listed) tags += " +trait:listed";
    c.push_back("tag " + g + tags);
}

/* A rune of one of the demo's kinds: its fields in order, then its tags. */
inline void thing(std::vector<std::string>& c, const std::string& glyph, const std::string& name,
                  const std::vector<std::pair<std::string, std::string>>& fields, const std::string& tags) {
    c.push_back("rune new " + glyph + " " + name);
    for (const auto& [k, v] : fields)
        if (!v.empty()) c.push_back("set " + name + " " + k + " " + q(v));
    c.push_back("tag " + name + " +type:" + glyph + (tags.empty() ? "" : " " + tags));
}

/* A drawn canvas, its first layer, and the regions on it. */
struct Region {
    const char* name;
    const char* label;
    double y1, x1, y2, x2;
    const char* gives;
    const char* color;
    const char* kind; // rect | ellipse
};
inline void canvas(std::vector<std::string>& c, const std::string& cv, const std::string& title, double w,
                   double h, double grid, const std::string& unit, const std::string& layer,
                   const std::string& layer_title, const std::string& rules_json, const std::vector<Region>& regions) {
    c.push_back("rune new canvas " + cv);
    c.push_back("set " + cv + " title " + q(title));
    c.push_back("set " + cv + " world \"plan\"");
    c.push_back("set " + cv + " width " + q(std::to_string((int)w)));
    c.push_back("set " + cv + " height " + q(std::to_string((int)h)));
    c.push_back("set " + cv + " grid " + q(std::to_string((int)grid)));
    c.push_back("set " + cv + " unit " + q(unit));
    c.push_back("tag " + cv + " +type:canvas");
    c.push_back("rune new map " + layer);
    c.push_back("set " + layer + " title " + q(layer_title));
    c.push_back("set " + layer + " source \"plan\"");
    c.push_back("set " + layer + " canvas " + q(cv));
    c.push_back("set " + layer + " channel " + q("cv_" + cv));
    c.push_back("set " + layer + " order \"0\"");
    c.push_back("set " + layer + " show_labels \"1\"");
    c.push_back("set " + layer + " no_overlap \"1\"");
    c.push_back("setjson " + layer + " rules '" + rules_json + "'");
    c.push_back("tag " + layer + " +type:map");
    for (const auto& r : regions) {
        const std::string n = r.name;
        c.push_back("rune new mapshape " + n);
        c.push_back("set " + n + " kind " + q(r.kind));
        c.push_back("set " + n + " geo1 " + q(geo(r.y1, r.x1)));
        c.push_back("set " + n + " geo2 " + q(geo(r.y2, r.x2)));
        c.push_back("set " + n + " label " + q(r.label));
        c.push_back("set " + n + " bestows " + q(r.gives));
        c.push_back("set " + n + " canvas " + q(cv));
        c.push_back(std::string("tag ") + n + " +type:mapshape" + (*r.color ? std::string(" +color:") + r.color : ""));
    }
    c.push_back("config set view.map.canvas " + q(cv));
}

/* ═════════════════════════════ THE WHISKERWOOD ═════════════════════════════
 * A D&D 5e campaign: a party of cats, each of a 5e race and class, the
 * creatures they meet, the places on a map of the Whiskerwood drawn in FEET,
 * the quests, the sessions on the calendar, and the treasure. */
inline Transcript campaign() {
    Transcript t;
    auto& k = t.kinds;
    kind(k, {"character", "Character", "Characters", "cat", "#7b4fa3", "Party",
             {{"race", "Race", ""}, {"klass", "Class", ""}, {"level", "Level", ""}, {"hp", "Hit points", ""},
              {"ac", "Armor class", ""}, {"str", "STR", ""}, {"dex", "DEX", ""}, {"con", "CON", ""},
              {"int", "INT", ""}, {"wis", "WIS", ""}, {"cha", "CHA", ""}, {"alignment", "Alignment", ""},
              {"background", "Background", ""}, {"player", "Played by", ""}, {"notes", "Notes", "multiline:60"}},
             true, false, false, "", "klass"});
    kind(k, {"creature", "Creature", "Creatures", "dragon", "#8b2e2e", "Bestiary",
             {{"type", "Type", ""}, {"size", "Size", ""}, {"cr", "Challenge rating", ""}, {"hp", "Hit points", ""},
              {"ac", "Armor class", ""}, {"speed", "Speed", ""}, {"alignment", "Alignment", ""},
              {"notes", "Notes", "multiline:60"}},
             true, false, false, "", "cr"});
    kind(k, {"place", "Place", "Places", "flag", "#3b7a57", "World",
             {{"what", "What it is", ""}, {"notes", "Notes", "multiline:60"}}, true, false, false, "", "what"});
    kind(k, {"quest", "Quest", "Quests", "scroll", "#b8860b", "Story",
             {{"giver", "Given by", ""}, {"reward", "Reward", ""}, {"status", "Status", ""},
              {"deadline", "Must be done by", "date"}, {"notes", "Notes", "multiline:60"}},
             false, true, false, "deadline", "status"});
    kind(k, {"session", "Session", "Sessions", "dice", "#4a6fa5", "Story",
             {{"date", "Played on", "date"}, {"number", "Session number", ""},
              {"summary", "What happened", "multiline:70"}},
             false, true, false, "date", "date"});
    kind(k, {"item", "Item", "Items", "wand", "#6a5acd", "Treasure",
             {{"rarity", "Rarity", ""}, {"attunement", "Needs attunement", ""}, {"holder", "Carried by", ""},
              {"notes", "Notes", "multiline:60"}},
             false, false, false, "", "rarity"});

    auto& c = t.data;
    c.push_back("use demo-org");
    canvas(c, "whiskerwood", "The Whiskerwood", 2400, 1600, 100, "ft", "campaign-map", "The party's map",
           R"([{"name":"the party","tags":["type:character"],"icon":"","color":"purple"},)"
           R"({"name":"creatures","tags":["type:creature"],"icon":"warning","color":"red"},)"
           R"({"name":"places","tags":["type:place"],"icon":"flag","color":"green"}])",
           {{"r-forest", "Whiskerwood Forest", 150, 100, 1150, 1100, "region:forest", "green", "rect"},
            {"r-hollow", "Catnip Hollow", 900, 1250, 1450, 1800, "region:town", "orange", "rect"},
            {"r-yarnspire", "Yarnspire", 200, 1500, 650, 1950, "region:yarnspire", "purple", "ellipse"},
            {"r-ruins", "Old Litterbox Ruins", 1150, 200, 1500, 800, "region:ruins", "gray", "rect"},
            {"r-docks", "Fishbone Docks", 1150, 1950, 1550, 2350, "region:docks", "blue", "rect"}});
    const char* G = "geo_cv_whiskerwood";
    struct Hero {
        const char *n, *title, *race, *klass, *lvl, *hp, *ac, *s, *d, *co, *in, *w, *ch, *al, *bg, *player;
        double y, x;
    };
    const Hero heroes[] = {
        {"mittens", "Mittens Quickpaw", "Tabaxi", "Rogue", "5", "33", "15", "8", "18", "12", "13", "12", "14",
         "Chaotic Good", "Urchin", "Sam", 1150, 1480},
        {"sir-pounce", "Sir Pounce-a-Lot", "Dragonborn", "Paladin", "5", "44", "18", "17", "10", "14", "8", "12", "16",
         "Lawful Good", "Noble", "Riley", 1160, 1520},
        {"velvet", "Duchess Velvet", "High Elf", "Wizard", "5", "27", "12", "8", "14", "13", "18", "12", "10",
         "Neutral Good", "Sage", "Jordan", 1140, 1560},
        {"grumbold", "Grumbold Furbeard", "Hill Dwarf", "Cleric (Life)", "5", "43", "18", "14", "8", "15", "10", "17",
         "12", "Lawful Good", "Acolyte", "Casey", 1180, 1500},
        {"bramble", "Bramble Tumbletail", "Lightfoot Halfling", "Ranger", "5", "39", "15", "10", "17", "14", "11", "15",
         "10", "Neutral Good", "Outlander", "Morgan", 1170, 1540},
        {"tansy", "Tansy Emberwhisk", "Tiefling", "Warlock (Fiend)", "5", "33", "13", "8", "14", "14", "12", "10", "18",
         "Chaotic Neutral", "Charlatan", "Avery", 1130, 1500},
        {"pip", "Pip Cogwhisker", "Rock Gnome", "Bard", "4", "27", "14", "8", "14", "12", "15", "10", "17",
         "Chaotic Good", "Entertainer", "Quinn", 1190, 1530},
    };
    for (const auto& h : heroes)
        thing(c, "character", h.n,
              {{"title", h.title}, {"race", h.race}, {"klass", h.klass}, {"level", h.lvl}, {"hp", h.hp}, {"ac", h.ac},
               {"str", h.s}, {"dex", h.d}, {"con", h.co}, {"int", h.in}, {"wis", h.w}, {"cha", h.ch},
               {"alignment", h.al}, {"background", h.bg}, {"player", h.player}, {G, geo(h.y, h.x)}},
              "+located +party region:town");
    struct Beast {
        const char *n, *title, *type, *size, *cr, *hp, *ac, *speed, *al, *notes, *where;
        double y, x;
    };
    const Beast beasts[] = {
        {"displacer-beast", "Displacer Beast", "Monstrosity", "Large", "3", "85", "13", "40 ft", "Lawful Evil",
         "A big cat that is never quite where it seems: the party's distant cousin, and not a friendly one.",
         "region:forest", 600, 500},
        {"rat-swarm", "Swarm of Rats", "Swarm of Tiny beasts", "Medium", "1/4", "24", "10", "30 ft", "Unaligned",
         "In the Catnip Tavern's cellar. The first quest.", "region:town", 1250, 1400},
        {"owlbear", "Owlbear", "Monstrosity", "Large", "3", "59", "13", "40 ft", "Unaligned",
         "Nests by the old oak. Does not care that you are a cat.", "region:forest", 400, 800},
        {"goblin-scouts", "Goblin scouts", "Humanoid (goblinoid)", "Small", "1/4", "7", "15", "30 ft", "Neutral Evil",
         "Four of them, camped at the edge of the ruins.", "region:ruins", 1300, 400},
        {"verdigrass", "Verdigrass the Young Green Dragon", "Dragon", "Large", "8", "136", "18", "40 ft, fly 80 ft",
         "Lawful Evil", "Hoards laser dots. Nobody knows how.", "region:yarnspire", 420, 1720},
        {"mimic", "Mimic", "Monstrosity (shapechanger)", "Medium", "2", "58", "12", "15 ft", "Neutral",
         "Looks like a cardboard box. Every cat's weakness.", "region:ruins", 1350, 650},
        {"will-o-wisp", "Will-o-wisp", "Undead", "Tiny", "2", "22", "19", "0 ft, fly 50 ft", "Chaotic Evil",
         "A light that moves like a laser pointer. Do not chase it.", "region:forest", 900, 300},
    };
    for (const auto& b : beasts)
        thing(c, "creature", b.n,
              {{"title", b.title}, {"type", b.type}, {"size", b.size}, {"cr", b.cr}, {"hp", b.hp}, {"ac", b.ac},
               {"speed", b.speed}, {"alignment", b.al}, {"notes", b.notes}, {G, geo(b.y, b.x)}},
              std::string("+located ") + b.where);
    struct Place {
        const char *n, *title, *what, *notes, *where;
        double y, x;
    };
    const Place places[] = {
        {"catnip-tavern", "The Catnip Tavern", "Tavern", "Where every quest starts. Old Tom pours the cream.",
         "region:town", 1200, 1450},
        {"yarnspire-tower", "Yarnspire Tower", "Wizard tower", "The archmage's tower, wound in enchanted yarn.",
         "region:yarnspire", 430, 1650},
        {"litterbox-ruins", "The Old Litterbox Ruins", "Ruin", "An ancient temple of sand. It smells of history.",
         "region:ruins", 1320, 500},
        {"fishbone-docks", "Fishbone Docks", "Harbour", "Boats, gulls and the best fish in the realm.",
         "region:docks", 1350, 2150},
        {"moonmilk-shrine", "Moonmilk Shrine", "Shrine", "A quiet clearing where the moon pools like milk.",
         "region:forest", 300, 400},
    };
    for (const auto& p : places)
        thing(c, "place", p.n, {{"title", p.title}, {"what", p.what}, {"notes", p.notes}, {G, geo(p.y, p.x)}},
              std::string("+located ") + p.where);
    thing(c, "quest", "q-missing-yarn",
          {{"title", "The Missing Yarn of Yarnspire"}, {"giver", "Archmage Purrcival"}, {"reward", "300 gp"},
           {"status", "Active"}, {"deadline", "2026-10-24"},
           {"notes", "The enchanted yarn that holds the tower up is unravelling. Someone pulled a thread."}},
          "+active");
    thing(c, "quest", "q-rats",
          {{"title", "Rats in the Cellar"}, {"giver", "Old Tom of the Catnip Tavern"},
           {"reward", "50 gp and free cream for life"}, {"status", "Done"}, {"deadline", "2026-10-03"}},
          "+done");
    thing(c, "quest", "q-laser-dots",
          {{"title", "The Dragon Who Hoards Laser Dots"}, {"giver", "The Docks Guild"}, {"reward", "A dragon hoard"},
           {"status", "Rumoured"}, {"deadline", "2026-11-14"}},
          "+rumoured");
    struct Sess {
        const char *n, *date, *num, *sum;
    };
    const Sess sessions[] = {
        {"session-1", "2026-10-03", "1", "The party meets at the Catnip Tavern and clears the cellar of rats."},
        {"session-2", "2026-10-10", "2", "Into the Whiskerwood. The owlbear wins a round; Grumbold wins it back."},
        {"session-3", "2026-10-17", "3", "To be played: the road to Yarnspire."},
        {"session-4", "2026-10-24", "4", "To be played."},
    };
    for (const auto& s : sessions)
        thing(c, "session", s.n,
              {{"title", std::string("Session ") + s.num}, {"date", s.date}, {"number", s.num}, {"summary", s.sum}}, "");
    struct Loot {
        const char *n, *title, *rarity, *att, *holder;
    };
    const Loot loot[] = {
        {"bag-of-holding", "Bag of Holding", "Uncommon", "No", "Mittens Quickpaw"},
        {"cloak-elvenkind", "Cloak of Elvenkind", "Uncommon", "Yes", "Bramble Tumbletail"},
        {"potions-healing", "Potions of Healing (3)", "Common", "No", "Grumbold Furbeard"},
        {"wand-missiles", "Wand of Magic Missiles", "Uncommon", "No", "Duchess Velvet"},
        {"scratching-post", "Scratching Post of Sharpness +1", "Rare (homebrew)", "Yes", "Sir Pounce-a-Lot"},
    };
    for (const auto& l : loot)
        thing(c, "item", l.n, {{"title", l.title}, {"rarity", l.rarity}, {"attunement", l.att}, {"holder", l.holder}},
              "");
    c.push_back("config set workspace.kind \"campaign\"");
    return t;
}

/* ═════════════════════════════════ WHISKER MART ════════════════════════════
 * A small cat grocery: the floor, the products on it, the vendors they come
 * from (Big Fish, M.E.O.W. Distribution...), the customers, the sales and the
 * purchase orders: the first shape of a point of sale. */
inline Transcript mart() {
    Transcript t;
    auto& k = t.kinds;
    kind(k, {"product", "Product", "Products", "fish", "#2f7d6d", "Store",
             {{"price", "Price", ""}, {"unit", "Sold by", ""}, {"sku", "SKU or barcode", ""}, {"stock", "In stock", ""},
              {"vendor", "Comes from", ""}, {"notes", "Notes", "multiline:60"}},
             true, false, false, "", "price"});
    kind(k, {"vendor", "Vendor", "Vendors", "truck", "#8a5a2b", "Store",
             {{"contact_name", "Who to call", ""}, {"phone", "Phone", ""}, {"email", "Email", ""},
              {"terms", "Payment terms", ""}, {"delivery_day", "Delivers on", ""}, {"notes", "Notes", "multiline:60"}},
             false, false, true, "", "terms"});
    kind(k, {"customer", "Customer", "Customers", "paw", "#b3592e", "People",
             {{"phone", "Phone", ""}, {"email", "Email", ""}, {"loyalty_points", "Loyalty points", ""},
              {"favorite", "Always buys", ""}, {"notes", "Notes", "multiline:60"}},
             false, false, false, "", "loyalty_points"});
    kind(k, {"sale", "Sale", "Sales", "receipt", "#3f6fae", "Till",
             {{"date", "Date", "date"}, {"customer", "Customer", ""}, {"items", "Items", "multiline:60"},
              {"total", "Total", ""}, {"payment", "Paid by", ""}},
             false, true, false, "date", "total"});
    kind(k, {"purchase_order", "Purchase order", "Purchase orders", "box", "#6b6b8a", "Till",
             {{"vendor", "Vendor", ""}, {"date", "Ordered on", "date"}, {"items", "Items", "multiline:60"},
              {"status", "Status", ""}, {"total", "Total", ""}},
             false, true, false, "date", "status"});

    auto& c = t.data;
    c.push_back("use demo-org");
    thing(c, "organization", "whisker-mart",
          {{"display_name", "Whisker Mart"}, {"notes", "A made-up cat grocery: the grocery workspace's demo."}},
          "+type:organization");
    canvas(c, "mart-floor", "Sales floor", 24, 16, 1, "m", "mart-layout", "Departments",
           R"([{"name":"fish","tags":["dept:fish"],"icon":"","color":"blue"},)"
           R"({"name":"dairy","tags":["dept:dairy"],"icon":"","color":"teal"},)"
           R"({"name":"treats","tags":["dept:treats"],"icon":"","color":"orange"},)"
           R"({"name":"greens","tags":["dept:greens"],"icon":"","color":"green"},)"
           R"({"name":"low stock","tags":["stock:low"],"icon":"warning","color":"red"}])",
           {{"r-fish", "Fish counter", 0.5, 1, 4, 10, "dept:fish", "blue", "rect"},
            {"r-dairy", "Cat milk and dairy", 0.5, 12, 3, 23, "dept:dairy", "teal", "rect"},
            {"r-greens", "Fresh greens", 5, 1, 13, 4, "dept:greens", "green", "rect"},
            {"r-aisle-1", "1 Treats", 5, 6.5, 12, 8.5, "aisle:1, dept:treats", "", "rect"},
            {"r-aisle-2", "2 Kibble and cans", 5, 10, 12, 12, "aisle:2, dept:food", "", "rect"},
            {"r-aisle-3", "3 Toys", 5, 13.5, 12, 15.5, "aisle:3, dept:toys", "", "rect"},
            {"r-aisle-4", "4 Litter and care", 5, 17, 12, 19, "aisle:4, dept:care", "", "rect"},
            {"r-checkout", "Checkout", 13.5, 8, 15.5, 22, "zone:checkout", "gray", "rect"}});
    struct Vendor {
        const char *n, *title, *who, *phone, *email, *terms, *day, *notes;
    };
    const Vendor vendors[] = {
        {"big-fish", "BIG FISH Seafood Supply", "Captain Mackerel", "555-0101", "orders@bigfish.example", "Net 15",
         "Tuesday and Friday", "Fresh fish, on ice, before the store opens."},
        {"meow-distribution", "M.E.O.W. Distribution", "Ms. Purrkins", "555-0102", "sales@meow.example", "Net 30",
         "Monday", "Mighty Excellent Order Wholesalers: dairy, treats and toys."},
        {"purrfect-produce", "Purrfect Produce Co.", "Sprout", "555-0103", "hello@purrfect.example",
         "Cash on delivery", "Every day", "Catnip and cat grass, grown down the road."},
        {"kibbleton-mills", "Kibbleton Mills", "Mr. Crunch", "555-0104", "mill@kibbleton.example", "Net 30",
         "Wednesday", "Kibble by the sack, cans by the case."},
        {"hiss-and-co", "Hiss and Co. Pet Care", "Dr. Fang", "555-0105", "care@hiss.example", "Net 30", "Thursday",
         "Litter, brushes and flea care."},
    };
    for (const auto& v : vendors)
        thing(c, "vendor", v.n,
              {{"title", v.title}, {"contact_name", v.who}, {"phone", v.phone}, {"email", v.email}, {"terms", v.terms},
               {"delivery_day", v.day}, {"notes", v.notes}},
              "");
    const char* G = "geo_cv_mart-floor";
    struct Product {
        const char *n, *title, *price, *unit, *stock, *vendor, *tags;
        double y, x;
    };
    const Product products[] = {
        {"salmon-fillet", "Salmon fillet", "12.99", "kg", "medium", "big-fish", "dept:fish", 2, 3},
        {"tuna-steak", "Tuna steak", "14.49", "kg", "low", "big-fish", "dept:fish", 2, 5.5},
        {"sardines", "Sardines, tinned", "2.19", "each", "high", "big-fish", "dept:fish", 2, 8},
        {"shrimp", "Shrimp, peeled", "9.99", "kg", "medium", "big-fish", "dept:fish", 3.2, 4},
        {"cat-milk", "Cat milk", "3.49", "litre", "high", "meow-distribution", "dept:dairy", 1.7, 14},
        {"cream", "Single cream", "2.29", "each", "medium", "meow-distribution", "dept:dairy", 1.7, 17},
        {"cheese-cubes", "Cheese cubes", "3.99", "each", "low", "meow-distribution", "dept:dairy", 1.7, 20},
        {"catnip", "Fresh catnip", "1.99", "bunch", "high", "purrfect-produce", "dept:greens", 7, 2.5},
        {"cat-grass", "Cat grass, potted", "4.50", "each", "medium", "purrfect-produce", "dept:greens", 10.5, 2.5},
        {"salmon-treats", "Salmon treats", "3.79", "each", "high", "meow-distribution", "aisle:1 dept:treats", 7,
         7.5},
        {"chicken-bites", "Chicken bites", "3.49", "each", "low", "meow-distribution", "aisle:1 dept:treats", 10,
         7.5},
        {"kibble-5kg", "Kibble, 5 kg", "24.99", "each", "medium", "kibbleton-mills", "aisle:2 dept:food", 7, 11},
        {"tuna-pate", "Tuna pate, can", "1.29", "each", "high", "kibbleton-mills", "aisle:2 dept:food", 10, 11},
        {"yarn-ball", "Yarn ball", "2.99", "each", "high", "meow-distribution", "aisle:3 dept:toys", 7, 14.5},
        {"feather-wand", "Feather wand", "6.49", "each", "medium", "meow-distribution", "aisle:3 dept:toys", 9, 14.5},
        {"laser-pointer", "Laser pointer", "8.99", "each", "low", "meow-distribution", "aisle:3 dept:toys", 11,
         14.5},
        {"litter-10kg", "Clumping litter, 10 kg", "11.99", "each", "medium", "hiss-and-co", "aisle:4 dept:care", 7,
         18},
        {"flea-comb", "Flea comb", "4.99", "each", "high", "hiss-and-co", "aisle:4 dept:care", 10, 18},
    };
    for (const auto& p : products) {
        std::string tags = std::string("+located +stock:") + p.stock + " +vendor:" + p.vendor;
        std::string tg, all = p.tags;
        for (char ch : all + " ") {
            if (ch == ' ') {
                if (!tg.empty()) tags += " +" + tg;
                tg.clear();
            } else {
                tg += ch;
            }
        }
        thing(c, "product", p.n,
              {{"title", p.title}, {"price", p.price}, {"unit", p.unit}, {"stock", p.stock}, {"vendor", p.vendor},
               {G, geo(p.y, p.x)}},
              tags);
    }
    struct Cust {
        const char *n, *title, *pts, *fav, *notes;
    };
    const Cust customers[] = {
        {"mrs-fluffington", "Mrs. Fluffington", "420", "Salmon fillet", "Comes in every Tuesday, when Big Fish delivers."},
        {"alley-tom", "Alley Tom", "85", "Sardines", "Pays in exact change. Mostly."},
        {"shadow", "Shadow", "210", "Laser pointer", "Has bought four laser pointers. Has caught none of the dots."},
        {"luna", "Luna", "35", "Cat milk", ""},
        {"biscuit", "Biscuit", "150", "Chicken bites", "Allergic to fish, says Biscuit. Buys fish anyway."},
    };
    for (const auto& cu : customers)
        thing(c, "customer", cu.n,
              {{"title", cu.title}, {"loyalty_points", cu.pts}, {"favorite", cu.fav}, {"notes", cu.notes}}, "");
    struct Sale {
        const char *n, *date, *cust, *items, *total, *pay;
    };
    const Sale sales[] = {
        {"sale-0001", "2026-10-01", "Mrs. Fluffington", "0.4 kg Salmon fillet\n1 Cat milk", "8.69", "Card"},
        {"sale-0002", "2026-10-01", "Alley Tom", "3 Sardines, tinned", "6.57", "Cash"},
        {"sale-0003", "2026-10-02", "Shadow", "1 Laser pointer\n1 Feather wand", "15.48", "Card"},
        {"sale-0004", "2026-10-03", "Biscuit", "2 Chicken bites\n1 Fresh catnip", "8.97", "Card"},
        {"sale-0005", "2026-10-05", "Luna", "2 Cat milk", "6.98", "Cash"},
        {"sale-0006", "2026-10-06", "Mrs. Fluffington", "0.5 kg Salmon fillet\n0.3 kg Tuna steak", "10.84", "Card"},
    };
    for (const auto& s : sales)
        thing(c, "sale", s.n,
              {{"title", std::string("Sale ") + (s.n + 5)}, {"date", s.date}, {"customer", s.cust},
               {"items", s.items}, {"total", s.total}, {"payment", s.pay}},
              "");
    thing(c, "purchase_order", "po-0001",
          {{"title", "PO 0001 to BIG FISH"}, {"vendor", "big-fish"}, {"date", "2026-10-06"},
           {"items", "10 kg Salmon fillet\n6 kg Tuna steak\n48 Sardines, tinned"}, {"status", "Sent"},
           {"total", "281.40"}},
          "+vendor:big-fish");
    thing(c, "purchase_order", "po-0002",
          {{"title", "PO 0002 to M.E.O.W."}, {"vendor", "meow-distribution"}, {"date", "2026-10-05"},
           {"items", "24 Cat milk\n12 Cheese cubes\n12 Laser pointers"}, {"status", "Delivered"},
           {"total", "167.64"}},
          "+vendor:meow-distribution");
    c.push_back("config set workspace.kind \"grocery\"");
    return t;
}

/* ═════════════════════════════════ THE HOUSE OF CATS ═══════════════════════
 * A home kept by its cats: the ground floor, its rooms (regions giving
 * `room:`), the smart devices in them, the residents, the inventory with
 * expiration dates on the calendar, and the chores. */
inline Transcript home() {
    Transcript t;
    auto& k = t.kinds;
    kind(k, {"device", "Device", "Devices", "lightbulb", "#c9a227", "Smart home",
             {{"brand", "Brand", ""}, {"model", "Model", ""}, {"protocol", "Talks over", ""}, {"state", "State", ""},
              {"battery", "Battery", ""}, {"notes", "Notes", "multiline:60"}},
             true, false, false, "", "state"});
    kind(k, {"resident", "Resident", "Residents", "cat", "#b3592e", "Household",
             {{"breed", "Breed", ""}, {"age", "Age", ""}, {"favorite_spot", "Favourite spot", ""},
              {"notes", "Notes", "multiline:60"}},
             true, false, false, "", "breed"});
    kind(k, {"supply", "Supply", "Supplies", "box", "#2f7d6d", "Inventory",
             {{"quantity", "How many", ""}, {"unit", "Of what", ""}, {"expires", "Expiration date", "date"},
              {"bought", "Bought on", "date"}, {"notes", "Notes", "multiline:60"}},
             true, true, false, "expires", "quantity"});
    kind(k, {"chore", "Chore", "Chores", "wrench", "#6b6b8a", "Household",
             {{"due", "Due", "date"}, {"every", "Repeats", ""}, {"who", "Whose job", ""},
              {"notes", "Notes", "multiline:60"}},
             false, true, false, "due", "due"});

    auto& c = t.data;
    c.push_back("use demo-org");
    canvas(c, "ground-floor", "Ground floor", 14, 10, 1, "m", "home-layout", "Rooms",
           R"([{"name":"devices","tags":["type:device"],"icon":"","color":"yellow"},)"
           R"({"name":"residents","tags":["type:resident"],"icon":"heart","color":"orange"},)"
           R"({"name":"supplies","tags":["type:supply"],"icon":"","color":"teal"},)"
           R"({"name":"needs attention","tags":["attention"],"icon":"warning","color":"red"}])",
           {{"r-living", "Living room", 0.3, 0.3, 5.5, 7, "room:living", "orange", "rect"},
            {"r-kitchen", "Kitchen", 0.3, 7.3, 4.5, 13.7, "room:kitchen", "green", "rect"},
            {"r-hall", "Hallway", 5.8, 0.3, 7, 13.7, "room:hall", "gray", "rect"},
            {"r-bedroom", "Bedroom", 7.3, 0.3, 9.7, 7, "room:bedroom", "purple", "rect"},
            {"r-bathroom", "Bathroom", 7.3, 7.3, 9.7, 10.5, "room:bathroom", "blue", "rect"},
            {"r-sunroom", "Sunroom", 4.8, 10.8, 9.7, 13.7, "room:sunroom", "yellow", "rect"}});
    const char* G = "geo_cv_ground-floor";
    struct Dev {
        const char *n, *title, *brand, *model, *proto, *state, *batt, *room, *extra;
        double y, x;
    };
    const Dev devices[] = {
        {"living-bulb", "Living room ceiling light", "LumiCat", "Bulb A19", "Zigbee", "On, 60%", "", "room:living", "",
         2.5, 3.5},
        {"kitchen-bulb", "Kitchen light", "LumiCat", "Bulb A19", "Zigbee", "Off", "", "room:kitchen", "", 2, 10},
        {"bedroom-lamp", "Bedside lamp", "LumiCat", "Lamp Mini", "Wi-Fi", "Off", "", "room:bedroom", "", 8, 1.5},
        {"feeder", "Smart feeder", "FeedPaw", "FeedPaw 2", "Wi-Fi", "Next meal 18:00", "80%", "room:kitchen", "", 3.5,
         12},
        {"fountain", "Water fountain", "AquaWhisk", "Flow 3", "Wi-Fi", "Running", "", "room:kitchen",
         "+attention", 3.5, 8.5},
        {"litter-robot", "Self-cleaning litter box", "LitterBot", "LB-4", "Wi-Fi", "Drawer 70% full", "",
         "room:bathroom", "", 8.5, 9.5},
        {"pet-camera", "Pet camera", "PawWatch", "Cam 360", "Wi-Fi", "Recording", "", "room:living", "", 0.8, 6.5},
        {"thermostat", "Thermostat", "Cozy", "T-1", "Matter", "21 C", "", "room:hall", "", 6.4, 2},
        {"heated-bed", "Heated cat bed (smart plug)", "PlugCat", "Mini Plug", "Zigbee", "On", "", "room:sunroom", "",
         7.5, 12.5},
        {"door-sensor", "Back door sensor", "Sentry", "DS-2", "Zigbee", "Closed", "15%", "room:sunroom", "+attention",
         9.3, 13.2},
        {"laser-toy", "Laser toy", "ZoomZoom", "Dot 1", "Bluetooth", "Scheduled 15:00", "60%", "room:living", "", 4,
         1.5},
    };
    for (const auto& d : devices)
        thing(c, "device", d.n,
              {{"title", d.title}, {"brand", d.brand}, {"model", d.model}, {"protocol", d.proto}, {"state", d.state},
               {"battery", d.batt}, {G, geo(d.y, d.x)}},
              std::string("+located ") + d.room + (*d.extra ? std::string(" ") + d.extra : "") + " +protocol:" +
                  (std::string(d.proto) == "Wi-Fi" ? "wifi" : std::string(d.proto) == "Zigbee" ? "zigbee"
                                                          : std::string(d.proto) == "Matter"   ? "matter"
                                                                                               : "bluetooth"));
    struct Res {
        const char *n, *title, *breed, *age, *spot, *room;
        double y, x;
    };
    const Res residents[] = {
        {"garfield", "Garfield", "Orange tabby", "9", "The sunroom, in the afternoon", "room:sunroom", 6.5, 11.5},
        {"whiskers", "Whiskers", "Tuxedo", "4", "On top of the fridge", "room:kitchen", 1, 13},
        {"luna", "Luna", "Russian Blue", "2", "Under the bed", "room:bedroom", 8.8, 4},
    };
    for (const auto& r : residents)
        thing(c, "resident", r.n,
              {{"title", r.title}, {"breed", r.breed}, {"age", r.age}, {"favorite_spot", r.spot}, {G, geo(r.y, r.x)}},
              std::string("+located ") + r.room);
    struct Sup {
        const char *n, *title, *qty, *unit, *expires, *bought, *room, *extra;
        double y, x;
    };
    const Sup supplies[] = {
        {"kibble-bag", "Kibble", "1", "5 kg bag", "2027-03-01", "2026-09-20", "room:kitchen", "", 1.2, 8},
        {"tuna-cans", "Tuna pate", "12", "cans", "2026-11-20", "2026-09-28", "room:kitchen", "", 1.2, 9},
        {"cat-milk", "Cat milk", "4", "200 ml cartons", "2026-10-15", "2026-10-02", "room:kitchen", "+attention", 1.2,
         10},
        {"catnip-jar", "Catnip", "1", "jar", "2026-10-30", "2026-08-15", "room:living", "", 5, 5},
        {"flea-treatment", "Flea treatment", "3", "doses", "2026-12-31", "2026-07-01", "room:bathroom", "", 9.2, 8},
        {"litter-bags", "Litter", "3", "10 kg bags", "", "2026-09-30", "room:bathroom", "", 9.2, 10},
        {"fountain-filters", "Fountain filters", "1", "filter", "", "2026-08-01", "room:kitchen", "+attention", 4,
         7.8},
        {"aa-batteries", "AA batteries", "8", "batteries", "2029-01-01", "2026-06-10", "room:hall", "", 6.4, 6},
    };
    for (const auto& s : supplies)
        thing(c, "supply", s.n,
              {{"title", s.title}, {"quantity", s.qty}, {"unit", s.unit}, {"expires", s.expires},
               {"bought", s.bought}, {G, geo(s.y, s.x)}},
              std::string("+located ") + s.room + (*s.extra ? std::string(" ") + s.extra : ""));
    struct Ch {
        const char *n, *title, *due, *every, *who;
    };
    const Ch chores[] = {
        {"empty-litter", "Empty the litter drawer", "2026-10-07", "Every 3 days", "The human"},
        {"door-battery", "Replace the back door sensor battery", "2026-10-09", "When it is low", "The human"},
        {"fountain-filter", "Change the fountain filter", "2026-10-12", "Every 2 weeks", "The human"},
        {"vet-garfield", "Garfield to the vet for his check-up", "2026-10-28", "Every year", "The human"},
        {"flea-dose", "Flea treatment for everyone", "2026-11-01", "Every month", "The human"},
    };
    for (const auto& ch : chores)
        thing(c, "chore", ch.n, {{"title", ch.title}, {"due", ch.due}, {"every", ch.every}, {"who", ch.who}}, "");
    c.push_back("config set workspace.kind \"home\"");
    return t;
}

} // namespace hormiga::demos
