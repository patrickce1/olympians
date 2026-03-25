// ItemTests.h
#ifndef __ITEM_TESTS_H__
#define __ITEM_TESTS_H__

#include <string>

/**
 * Static test suite for item JSON parsing and house-scaling loading.
 */
class ItemTests {
public:
    /**
     * Loads items/houses/enemies fixtures from JSON and runs all item tests.
     *
     * @param itemsJsonPath   Path to items.json (e.g. "json/items.json")
     * @param housesJsonPath  Path to houses.json (e.g. "json/houses.json")
     * @param enemiesJsonPath Path to enemies.json (e.g. "json/enemies.json")
     */
    static void runAll(const std::string& itemsJsonPath,
                       const std::string& housesJsonPath,
                       const std::string& enemiesJsonPath);
};

#endif /* __ITEM_TESTS_H__ */
