
#include "sv_document.hpp"

#include "slang/text/SourceManager.h"
#include "uri.hh"


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

            //! Holds the file to blackboxes associations.
            std::unordered_map<std::filesystem::path, std::vector<const ModuleBlackBox*> > _path_to_bb;

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
             * @brief Add a given file to the cache but do not process it
             * 
             * @param fpath path to the file to process.
             * @param in_prj true if the file is to be added to the project.
             */
            void record_file(const std::filesystem::path& fpath, bool in_prj);


            /**
             * @brief Get the uri path associated to the given file path.
             * If an URI is already recorded, use it. Otherwise, build it.
             * 
             * @param fpath Filesystem path to lookup/convert.
             */
            uri get_uri(const std::filesystem::path& fpath) const;


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
             * @brief Cleanly removes a file from the cache, thus removing all references
             * in all sub-containers.
             * 
             * @param fpath file to remove.
             */
            void remove_file(const std::filesystem::path& fpath);
    };

};