#pragma once

#include <optional>
#include <string>

namespace eka2l1::epoc {
    /*! \brief Initialize the panic descriptions.
     *
     * The function try to load the panic.json file at the same folder as
     * the executable, to a YAML node. From YAML node, function can get action
     * and descriptions of panic there.
     * 
     * There are two types of action: default and script. While default will returns
     * a string description, script will returns nothing, and assumes a script already
     * installed for handling panic (hooking using decorator @emulatorPanicHook(panicCategory),
     * from symemu2.events).
     *
     * \returns True if the loading performs successfully
    */
    bool init_panic_descriptions();

    /*! \brief Get the attribute, check if the panic category action is default or script. 
     *
     * A panic will be handle default by getting the info of the panic code from specific panic
     * category and print it along with error code and category. If the category doesn't exist,
     * this function still returns true, but the panic info string will not be available.
     *
     * \param panic_category The panic category to check action.
    */
    bool is_panic_category_action_default(const std::string &panic_category);

    /*! \brief Get the panic description of a panic code from a panic category. 
     *
     * This function will immediately returns none if the YAML node is not loaded. Otherwise, it will
     * lurk up the YAML main node and get the panic description string, if the category handling
     * is marked as default.
     *
     * \param category The panic category.
     * \param code The panic code.
     *
     * \returns An optional object contains the panic description string if success.
    */
    std::optional<std::string> get_panic_description(const std::string &category, const int code);
}