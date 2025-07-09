
#pragma once 


#include "slang/text/SourceManager.h"
#include "uri.hh"
#include "visitor_module_bb.hpp"


#include <memory>

#include <string>
#include <filesystem>
#include <set>

#include <unordered_map>
namespace diplomat::cache
{
    /**
     * @brief This class aim to handle store high-level files informations.
     * This means the associations file <-> modules, blackboxes and so on.
     * 
     * The target is to do a maximum of lazy-evaluation as to avoid a huge lag
     * at some point.
     */
    class DiplomatDocumentCache
    {
        protected : 
            //! Its very own source manager as to not pollute the actual LSP SM.
            //! If a batch read is made, it should be possible to discard it.
            std::unique_ptr<slang::SourceManager> _sm;

            //! Main storage that actually own the BB.
            //! This is indexed by address so it will be easy to store in other LUT.
            std::unordered_map<std::intptr_t, std::unique_ptr< ModuleBlackBox> > _bb_storage;

            //! List of recorded files
            std::set<std::filesystem::path> _prj_files;
            std::set<std::filesystem::path> _ws_files;

            std::unordered_map<std::filesystem::path, std::filesystem::file_time_type> _processed_timestamp;

            //! Holds the file to blackboxes associations.
            std::unordered_map<std::filesystem::path, std::vector<const ModuleBlackBox*> > _path_to_bb;
            
            //! Reverse lookup of a blackbox to its originating file.
            std::unordered_map<const ModuleBlackBox*, std::filesystem::path> _bb_to_path;

            //! Sometimes the client has a URI that differs from what the server would provide
            //! (In example, symbolic links). This table record actual URI send by the 
            //! client when opening a file (it is irrelevant otherwise).
            std::unordered_map<std::filesystem::path, uri> _doc_path_to_client_uri;

            //! Module name to BB binding, specific to the project and on the whole 
            //! workspace.
            std::unordered_map<std::string, const ModuleBlackBox*> _prj_module_to_bb;
            std::unordered_map<std::string, std::set<const ModuleBlackBox*>> _ws_module_to_bb;

            static inline std::filesystem::path _standardize_path(const std::filesystem::path& fpath) 
            {return std::filesystem::weakly_canonical(fpath);};

        public : 

            //DiplomatDocumentCache();

            /**
             * @brief Removes references from project files
             */
            void clear_project();

            /**
             * @brief Refresh the internal state of the cache for all updated files.
             * 
             */
            void refresh(bool prj_only);



            /**
             * @brief Add a given file to the cache but do not process it
             * 
             * @param fpath path to the file to process.
             * @param in_prj true if the file is to be added to the project.
             */
            void record_file(const std::filesystem::path& fpath, bool in_prj);


            /**
             * @brief Get a blackbox, looked up by the module name it defines
             * 
             * @param modname module name to lookup.
             * @return const ModuleBlackBox* Found BB if any, `nullptr` otherwise.
             * 
             * @note If multiple black boxes are found, return (by priority) :
             *  1. A module registered in the project.
             *  2. The first found BB in the workspace.
             * 
             *  \todo Use the BB signature for more accurate lookup.
             */
            const ModuleBlackBox* get_bb_by_module(const std::string& modname) const ;


            /**
             * @brief Get the list of blackboxes given a file path 
             * 
             * @param fpath The file path to lookup
             * @return const std::vector<const ModuleBlackBox*>* The recorded list of black-boxes or nullptr if the lookup failed.
             */
            const std::vector<const ModuleBlackBox*>* get_bb_by_file(const std::filesystem::path& fpath) const ;

            /**
             * @brief Get the uri path associated to the given file path.
             * If an URI is already recorded, use it. Otherwise, build it.
             * 
             * @param fpath Filesystem path to lookup/convert.
             */
            uri get_uri(const std::filesystem::path& fpath) const;


            /**
             * @brief Get the files list for the whole workspace
             * 
             * @return const std::set<std::filesystem::path>& a reference to the workspace file listing.
             */
            inline const std::set<std::filesystem::path>& get_files_ws() const
            {return _ws_files;};

            /**
             * @brief Get the files list for the project
             * 
             * @return const std::set<std::filesystem::path>& a reference to the project file listing. 
             */
            inline const std::set<std::filesystem::path>& get_files_prj() const
            {return _prj_files;};

            /**
             * @brief Get the file from a module name
             * 
             * @param module module blaackbox pointer to lookup
             * @return std::filesystem::path associated with the module blackbox .
             */
            inline std::filesystem::path get_file_from_module(const ModuleBlackBox* module) const
            {return _bb_to_path.at(module);};


            /**
             * @brief The the whole file to module binding of the cache
             * 
             * @return the map between the file path and the list of BB included.
             */
            inline const std::unordered_map<std::filesystem::path, std::vector<const ModuleBlackBox*> >& get_modules() const 
            {return _path_to_bb;};

            /**
             * @brief Get a constant pointer to the uri bindings object
             * 
             * This is to transmit to external component that would require 
             * this mapping while avoiding the dependency to diplomat.
             * 
             * @warning if possible, prefers using {@link get_uri} instead.
             * 
             * @return const std::unordered_map<std::filesystem::path, uri>* const 
             */
            inline const std::unordered_map<std::filesystem::path, uri>* get_uri_bindings() const
            {return &_doc_path_to_client_uri;};


            /**
             * @brief Returns true if the module name passed in argument is found
             * within the project.
             * 
             * @param modname Module name to lookup
             * @return true if the module is found within the project.
             */
            inline const bool got_module_in_project(const std::string& modname) const
            {return _prj_module_to_bb.contains(modname);};

            /**
             * @brief Process the given file, 
             * This will parse and generate the BB for the given file, then store it.
             * 
             * @note If the file is not recorded, this function will record it according to \p in_prj   .
             * This function is not able to remove a file from the project.
             * 
             * @param fpath path of the file to process.
             * @param in_prj If this file is to be recorded, record it in the project
             */
            void process_file(const std::filesystem::path& fpath, bool in_prj = false);

            /**
             * @brief Attempt to match an URI with a recorded file in order to fill 
             * {@link _doc_path_to_client_uri} .
             * 
             * @param client_uri The URI sent by the client.
             * @return the bound absolute path
             */
            std::filesystem::path record_uri(const uri& client_uri);
    

            /**
             * @brief Cleanly removes a file from the cache, thus removing all references
             * in all sub-containers.
             * 
             * @param fpath file to remove.
             * 
             * @note It is safe to call this method on a path that has not yet been recorded.
             * In this case, it will do nothing. 
             */
            void remove_file(const std::filesystem::path& fpath);
    };


};