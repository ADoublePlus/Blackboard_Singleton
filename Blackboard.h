#pragma once

#include <typeinfo>
#include <unordered_map>
#include <mutex>
#include <assert.h>
#include <stdexcept>

namespace Utilities
{
    // Forward declare the base type of the data storage object
    namespace Templates
    {
        class Base_Map;
    }

    // Define alias' for the different types of event callbacks that can be defined
    template <typename T> using EventKeyCallback = void(*)(const std::string&);
    template <typename T> using EventValueCallback = void(*)(const T&);
    template <typename T> using EventKeyValueCallback = void(*)(const std::string&, const T&);

    /*
        Purpose:
            Provide a singleton location for a user to store generic data.

            Callback functionality will be implemented to allow listening for when specific keyed data is changed.

            The create and destroy functions must be called prior to use.

        Warning:
            Data types stored on the blackboard must have valid default and copy constructors defined as well as the assignment operator.

            Only one callback event of each type will be stored for each key of every value type.
     */
    class Blackboard
    {
        /*---------- Singleton Values ----------*/
        static Blackboard* m_Instance;
        Blackboard() = default;
        ~Blackboard() = default;

         /*---------- Variables ----------*/
         // Store a map of all the different value types
         std::unordered_map<size_t, Templates::Base_Map*> m_DataStorage;

         // Store a mutex for locking data when in use
         std::recursive_mutex m_DataLock;

         // Convert a template type into a unique ID value
         template <typename T> inline size_t templateToID() const;

         // Ensure a Value_Map object exists for a specific type
         template <typename T> inline size_t supportType();

         public:
            // Creation/destruction
            static bool create();
            static void destroy();

            // Data reading/writing
            template <typename T> static void write(const std::string& p_Key, const T& p_Value, bool p_RaiseCallbacks = true);
            template <typename T> static const T& read(const std::string& p_Key);
            template <typename T> static void wipeTypeKey(const std::string& p_Key);
                                  static void wipeKey(const std::string& p_Key);
                                  static void wipeBoard(bool p_WipeCallbacks = false);
            
            // Callback functions
            template <typename T> static void subscribe(const std::string& p_Key, EventKeyCallback<T> pCb);
            template <typename T> static void subscribe(const std::string& p_Key, EventValueCallback<T> pCb);
            template <typename T> static void subscribe(const std::string& p_Key, EventKeyValueCallback<T> pCb);
            template <typename T> static void unsubscribe(const std::string& p_Key);
                                  static void unsubscribeAll(const std::string& p_Key);

            // Getters
            static inline bool isReady() { return (m_Instance != nullptr); }
    };

    namespace Templates
    {
        /*
            Purpose:
                Provide a base point for the templated Value_Map objects to inherit from.
                This allows the blackboard to store pointers to the templated versions for storing data.
         */
        class Base_Map
        {
            protected:
                // Set the Value_Map to be a friend of the blackboard to allow for construction/destruction of the object
                friend class Blackboard;

                // Privatise the constructor/destructor to prevent external use
                Base_Map() = default;
                virtual ~Base_Map() = 0;

                // Provide virtual methods for wiping keyed information
                inline virtual void wipeKey(const std::string& p_Key) = 0;
                inline virtual void wipeAll() = 0;
                inline virtual void unsubscribe(const std::string& p_Key) = 0;
                inline virtual void clearAllEvents() = 0;
        };

        // Define the default destructor for the Base_Map's pure virtual destructor
        inline Base_Map::~Base_Map() {}

        /*
            Purpose:
                Store templated data type information for recollection and use within the Blackboard singleton object.
         */
        template <typename T>
        class Value_Map : Base_Map
        {
            protected:
                // Set the Value_Map to be a friend of the blackboard to allow for construction/destruction of the object
                friend class Blackboard;

                /*---------- Variables ----------*/
                // Store a map of the values for this type
                std::unordered_map<std::string, T> m_Values;

                // Store maps for callback events
                std::unordered_map<std::string, EventKeyCallback<T>> m_KeyEvents;
                std::unordered_map<std::string, EventValueCallback<T>> m_ValueEvents;
                std::unordered_map<std::string, EventKeyValueCallback<T>> m_PairEvents;

                 /*---------- Functions ----------*/
                 // Privatise the constructor/destructor to prevent external use
                 Value_Map() = default;
                 ~Value_Map() override {}

                 // Override the functions used to remove keyed information
                 inline void wipeKey(const std::string& p_Key) override;
                 inline void wipeAll() override;
                 inline void unsubscribe(const std::string& p_Key) override;
                 inline void clearAllEvents() override;
        };  
    }

    #pragma region Template Definitions
    #pragma region Blackboard
    /*
        Blackboard : templateToID<T> - Convert the template type T to a unique hash code.

        template T - A generic, non void type.

        return size_t - Returns the ID as a size_t value.
     */
    template <typename T>
    inline size_t Utilities::Blackboard::templateToID() const
    {
        // Gather type info 
        const std::type_info& type = typeid(T);

        // Return the hash code
        return type.hash_code();
    }

    /*
        Blackboard - supportType<T> - Using the type of the template ensure there is a Value_Map to support holding data of its type.

        template T - A generic, non void type.

        return size_t - Returns the unique hash code for the template type T.
     */
    template <typename T>
    inline size_t Utilities::Blackboard::supportType()
    {
        // Retrieve the hash code for the specific type
        size_t key = templateToID<T>();

        // If there isn't an entry for the hash code, create a new map
        if (m_DataStorage.find(key) == m_DataStorage.end())
        {
            m_DataStorage[key] = new Utilities::Templates::Value_Map<T>();
        }

        // Return key
        return key;
    }

    /*
        Blackboard : write<T> - Write a data value to the Blackboard.

        template T - A generic, non void type.

        param[in] p_Key - The key value to save the data value at.
        param[in] p_Value - The data value to be saved to the key location.
        param[in] p_RaiseCallbacks - A flag to indicate if callback events should be raised (default true).
     */
    template <typename T>
    inline void Utilities::Blackboard::write(const std::string& p_Key, const T& p_Value, bool p_RaiseCallbacks)
    {
        // Ensure the singleton has been created
        assert(m_Instance);

        // Lock the data
        std::lock_guard<std::recursive_mutex> guard(m_Instance->m_DataLock);

        // Ensure the key for this type is supported
        size_t key = m_Instance->supportType<T>();

        // Cast Value_Map to type of T 
        Utilities::Templates::Value_Map<T>* map = (Utilities::Templates::Value_Map<T>*)(m_Instance->m_DataStorage[key]);

        // Copy the data value across
        map->m_Values[p_Key] = p_Value;

        // Check event flag
        if (p_RaiseCallbacks)
        {
            // Check for events to raise
            if (map->m_KeyEvents.find(p_Key) != map->m_KeyEvents.end() && map->m_KeyEvents[p_Key])
                map->m_KeyEvents[p_Key](p_Key);

            if (map->m_ValueEvents.find(p_Key) != map->m_ValueEvents.end() && map->m_ValueEvents[p_Key])
                map->m_ValueEvents[p_Key](map->m_Values[p_Key]);

            if (map->m_PairEvents.find(p_Key) != map->m_PairEvents.end() && map->m_PairEvents[p_Key])
                map->m_PairEvents[p_Key](p_Key, map->m_Values[p_Key]);
        }
    }

    /*
        Blackboard : read<T> - Read the value of a key value from the Blackboard.

        template T - A generic, non void type.

        param[in] p_Key - The key value to read the data value of.

        return const T& - Returns a constant reference to the data type of T.
     */
    template <typename T>
    inline const T& Utilities::Blackboard::read(const std::string& p_Key)
    {
        // Ensure the singleton has been created
        assert(m_Instance);

        // Lock the data
        std::lock_guard<std::recursive_mutex> guard(m_Instance->m_DataLock);

        // Ensure the key for this type is supported
        size_t key = m_Instance->supportType<T>();

        // Cast Value_Map to the type of T.
        Utilities::Templates::Value_Map<T>* map = (Utilities::Templates::Value_Map<T>*)(m_Instance->m_DataStorage[key]);

        // Return value at key location
        return map->m_Values[p_Key];
    }

    /*
        Blackboard : wipeTypeKey - Wipe the value stored at a specific key for the specified type.

        param[in] p_Key - A string object containing the key of the value(s) to remove.
     */
    template <typename T>
    inline void Utilities::Blackboard::wipeTypeKey(const std::string& p_Key)
    {
        // Ensure the singleton has been created
        assert(m_Instance);

        // Lock the data
        std::lock_guard<std::recursive_mutex> guard(m_Instance->m_DataLock);

        // Ensure the key for this type is supported
        size_t key = m_Instance->supportType<T>();

        // Cast Value_Map to the type of T.
        Utilities::Templates::Value_Map<T>* map = (Utilities::Templates::Value_Map<T>*)(m_Instance->m_DataStorage[p_Key]);

        // Wipe the key from the Value_Map
        map->wipeKey(p_Key);
    }

    /*
        Blackboard : subscribe<T> - Set the callback event for a specific key value on a type of data.

        template T - A generic, non void type.

        param[in] p_Key - The key to assign the callback event to.
        param[in] pCb - A function pointer that takes in a constant string reference as its only parameter.
     */
    template <typename T>
    inline void Utilities::Blackboard::subscribe(const std::string& p_Key, EventKeyCallback<T> pCb)
    {
        // Ensure the singleton has been created
        assert(m_Instance);

        // Lock the data
        std::lock_guard<std::recursive_mutex> guard(m_Instance->m_DataLock);

        // Ensure key for this type is supported
        size_t key = m_Instance->supportType<T>();

        // Cast Value_Map to the type of T
        Utilities::Templates::Value_Map<T>* map = (Utilities::Templates::Value_Map<T>*)(m_Instance->m_DataStorage[key]);

        // Set the event callback
        map->m_KeyEvents[p_Key] = pCb;
    }

    /*
        Blackboard : subscribe<T> - Set the callback event for a specific key value on a type of data.

        template T - A generic, non void type.

        param[in] p_Key - The key to assign the callback event to.
        param[in] pCb - A function pointer that takes in a constant reference to the new value that was assigned as its only parameter.
     */
    template <typename T>
    inline void Utilities::Blackboard::subscribe(const std::string& p_Key, EventValueCallback<T> pCb)
    {
        // Ensure the singleton has been created
        assert(m_Instance);

        // Lock the data
        std::lock_guard<std::recursive_mutex> guard(m_Instance->m_DataLock);

        // Ensure key for this type is supported
        size_t key = m_Instance->supportType<T>();

        // Cast Value_Map to the type of T
        Utilities::Templates::Value_Map<T>* map = (Utilities::Templates::Value_Map<T>*)(m_Instance->m_DataStorage[key]);

        // Set the event callback
        map->m_ValueEvents[p_Key] = pCb;
    }

    /*
        Blackboard : subscribe<T> - Set the callback event for a specific key value on a type of data.

        template T - A generic, non void type.

        param[in] p_Key - The key to assign the callback event to.
        param[in] pCb - A function pointer that takes in a constant string reference and a constant reference to the new value that was assigned as its only parameters.
     */
    template <typename T>
    inline void Utilities::Blackboard::subscribe(const std::string& p_Key, EventKeyValueCallback<T> pCb)
    {
        // Ensure the singleton has been created
        assert(m_Instance);

        // Lock the data
        std::lock_guard<std::recursive_mutex> guard(m_Instance->m_DataLock);

        // Ensure key for this type is supported
        size_t key = m_Instance->supportType<T>();

        // Cast Value_Map to the type of T
        Utilities::Templates::Value_Map<T>* map = (Utilities::Templates::Value_Map<T>*)(m_Instance->m_DataStorage[key]);

        // Set the event callback
        map->m_PairEvents[p_Key] = pCb;
    }

    /*
        Blackboard : unsubscribe - Unsubscribe all events associated with a key value for a specific type.

        param[in] p_Key - The key to remove the callback event from.
     */
    template <typename T>
    inline void Utilities::Blackboard::unsubscribe(const std::string& p_Key)
    {
        // Ensure the singleton has been created
        assert(m_Instance);

        // Lock the data
        std::lock_guard<std::recursive_mutex> guard(m_Instance->m_DataLock);

        // Ensure key for this type is supported
        size_t key = m_Instance->supportType<T>();

        // Cast Value_Map to the type of T
        Utilities::Templates::Value_Map<T>* map = (Utilities::Templates::Value_Map<T>*)(m_Instance->m_DataStorage[key]);

        // Pass the unsubscribe key to Value_Map
        map->unsubscribe(p_Key);
    }
    #pragma endregion

    #pragma region Value_Map
    /*
        Value_Map<T> : wipeKey - Clear the value associated with a key value.

        template T - A generic, non void type.

        param[in] p_Key - The key value to clear the entry of.
     */
    template <typename T>
    inline void Utilities::Templates::Value_Map<T>::wipeKey(const std::string& p_Key)
    {
        m_Values.erase(p_Key);
    }

    /*
        Value_Map<T> : wipeAll - Erase all data stored in the map.

        template T - A generic, non void type.
     */
    template <typename T>
    inline void Utilities::Templates::Value_Map<T>::wipeAll()
    {
        m_Values.clear();
    }

    /*
        Value_Map<T> : unsubscribe - Remove all callback events associated with a key value.

        template T - A generic, non void type.

        param[in] p_Key - The key to wipe all callback events associated with.
     */
    template <typename T>
    inline void Utilities::Templates::Value_Map<T>::unsubscribe(const std::string& p_Key)
    {
        // Find iterators
        auto key = m_KeyEvents.find(p_Key);
        auto val = m_ValueEvents.find(p_Key);
        auto pair = m_PairEvents.find(p_Key);

        // Remove the callbacks
        if (key != m_KeyEvents.end())
            m_KeyEvents.erase(key);

        if (key != m_ValueEvents.end())
            m_ValueEvents.erase(val);

        if (key != m_PairEvents.end())
            m_PairEvents.erase(pair);
    }

    /*
        Value_Map<T> : clearAllEvents - Clear all event callbacks stored within the Value_Map.

        template T - A generic, non void type.
     */
    template <typename T>
    inline void Utilities::Templates::Value_Map<T>::clearAllEvents()
    {
        // Clear all event maps
        m_KeyEvents.clear();
        m_ValueEvents.clear();
        m_PairEvents.clear();
    }
    #pragma endregion
    #pragma endregion
}

/* Object Definition */
#ifdef _BLACKBOARD_SINGLETON_
// Define the Blackboards static singleton instance
Utilities::Blackboard* Utilities::Blackboard::m_Instance = nullptr;

/*
    Blackboard : create - Initialize the Blackboard singleton for use.

    return bool - Returns true if the Blackboard was initialised successfully.
 */
bool Utilities::Blackboard::create()
{
    // Check if the Blackboard has already been setup
    if (isReady())
    {
        destroy();
    }

    // Create instance
    m_Instance = new Blackboard();

    // Return success state
    return (m_Instance != nullptr);
}

/*
    Blackboard : destroy - Deallocate all data associated with the Blackboard and destroy the singleton instance.
 */
void Utilities::Blackboard::destroy()
{
    // Check if an instance is available to destroy
    if (m_Instance)
    {
        // Lock the data values
        m_Instance->m_DataLock.lock();

        // Delete all Value_Map values
        for (auto pair : m_Instance->m_DataStorage)
        {
            delete pair.second;
        }

        // Unlock the data
        m_Instance->m_DataLock.unlock();

        // Delete singleton
        delete m_Instance;

        // Reset instance pointer
        m_Instance = nullptr;
    }
}

/*
    Blackboard : wipeKey - Clear all data associated with the passed in key value.

    param[in] p_Key - A string object containing the key of the value(s) to remove.
 */
void Utilities::Blackboard::wipeKey(const std::string& p_Key)
{
    // Ensure singleton has been created
    assert(m_Instance);

    // Lock the data
    std::lock_guard<std::recursive_mutex> guard(m_Instance->m_DataLock);

    // Loop through the different type collections
    for (auto pair : m_Instance->m_DataStorage)
    {
        pair.second->wipeKey(p_Key);
    }
}

/*
    Blackboard : wipeBoard - Clear all data stored on the Blackboard.

    param[in] p_WipeCallbacks - Flags if all set event callbacks should be cleared as well as the values (default false).
 */
void Utilities::Blackboard::wipeBoard(bool p_WipeCallbacks)
{
    // Ensure singleton has been created
    assert(m_Instance);

    // Lock the data
    std::lock_guard<std::recursive_mutex> guard(m_Instance->m_DataLock);

    // Loop through all stored Value_Maps
    for (auto pair : m_Instance->m_DataStorage)
    {
        // CLear data values
        pair.second->wipeAll();

        // Clear callbacks
        if (p_WipeCallbacks)
        {
            pair.second->clearAllEvents();
        }
    }
}

/*
    Blackboard : unsubscribeAll - Remove the associated callback events for a key from every type map.

    param[in] p_Key - The key to remove the callback events from.
 */
void Utilities::Blackboard::unsubscribeAll(const std::string& p_Key)
{
    // Ensure singleton has been created
    assert(m_Instance);

    // Lock the data
    std::lock_guard<std::recursive_mutex> guard(m_Instance->m_DataLock);

    // Loop through all stored Value_Maps
    for (auto pair : m_Instance->m_DataStorage)
    {
        pair.second->unsubscribe(p_Key);
    }
}
#endif