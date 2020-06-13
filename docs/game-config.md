Components are used to retrieve values for an architecture to use. Architectures are a way to bind together components into the SVR system.

Keys with 'config' in their name are ones that can be added without code modifications. Keys with 'code' in their name requrie code changes to work.

Each game is built up from a set of components. The architecture connects the components together to create the system. Games may have special handling.

List of code components:

* *Pattern* - Retrieves an address by scanning a region of memory for a particular pattern. Supports arbitrary offsets and following relative jump instructions.
* *CreateInterface* - Retrieves a pointer to an interface in some library.
* *Virtual* - Retrieves an address of a virtual function from pointer.
* *Export* - Retrieves an address of an exported function in some library.
* *Offset* - Constant value that denotes an offset. Can be used to note an offset in a structure for example.

Games have an unique identifier and a display name.

A game must list its required libraries so SVR knows when to initialize itself. SVR cannot initialize before all required libraries have been loaded.

Signatures are used to match a corresponding implementation in the code. If no matching implementation is found, initialization will fail.

Components that are in the game config are views of the underlying code component. Game config component views can be modified freely as long as they have an unique identifier. Code changes are required if a new code component is needed, or if the signature needs change.
