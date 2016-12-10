#include "BinaryFile.h"

class BinaryFileStub : public BinaryFile {
public:
	                    BinaryFileStub();
	virtual            ~BinaryFileStub() { }

	        bool        GetNextMember() { return false; }  // Load next member of archive
	virtual bool        Open(const char *sName) { return false; }  // Open for r/w; pv
	virtual void        Close() { }               // Close file opened with Open()
	virtual void        UnLoad() { }              // Unload the image
	virtual LOADFMT     getFormat() const;        // Get format (e.g. LOADFMT_ELF)
	virtual MACHINE     getMachine() const;       // Get machine (e.g. MACHINE_SPARC)
	virtual const char *getFilename() const { return m_pFileName; }

	virtual bool        isLibrary() const;
	virtual std::list<const char *> getDependencyList();
	virtual ADDRESS     getImageBase();
	virtual size_t      getImageSize();

	// Header functions
	virtual ADDRESS     GetFirstHeaderAddress();  // Get ADDRESS of main header
	virtual ADDRESS    *GetImportStubs(int &numImports);

//
//  --  --  --  --  --  --  --  --  --  --  --
//

	// Internal information
	// Dump headers, etc
	virtual bool        DisplayDetails(const char *fileName, FILE *f = stdout);

	// Analysis functions
	virtual std::list<SectionInfo *> &getEntryPoints(const char *pEntry = "main");
	virtual ADDRESS     getMainEntryPoint();
	virtual ADDRESS     getEntryPoint();

	// Get a map from ADDRESS to const char*. This map contains the native
	// addresses and symbolic names of global data items (if any) which are
	// shared with dynamically linked libraries. Example: __iob (basis for
	// stdout).The ADDRESS is the native address of a pointer to the real
	// dynamic data object.
	// The caller should delete the returned map.
	virtual std::map<ADDRESS, const char *> *GetDynamicGlobalMap();

	// Not meant to be used externally, but sometimes you just
	// have to have it.
	        const char *GetStrPtr(int idx, int offset);  // Calc string pointer

	// Similarly here; sometimes you just need to change a section's
	// link and info fields
	// idx is the section index; link and info are indices to other
	// sections that will be idx's sh_link and sh_info respectively
	        void        SetLinkAndInfo(int idx, int link, int info);

	        const char *m_pFileName;  // Pointer to input file name

protected:
	virtual bool        RealLoad(const char *sName);  // Load the file; pure virtual
	virtual bool        PostLoad(void *handle);       // Called after loading archive member
};
