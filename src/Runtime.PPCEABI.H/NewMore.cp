// The retail link stripped every NewMore function. What survives is the weak
// std::exception vtable, destructor and what(), which later runtime units use.
namespace std {

class exception {
public:
    exception() {}
    virtual ~exception() {}
    virtual const char* what() const { return "exception"; }
};

}  // namespace std

// Instantiates std::exception so its weak members are emitted here; the linker
// strips this function itself.
void __NewMore_exception_anchor(void) {
    std::exception e;
}
