template<class T>
class MyOptional : public std::optional<T>
{
    public:
    using std::optional<T>::optional;
    using std::optional<T>::operator=;

    constexpr T val() 
    {
       return std::optional<T>::value_or(T());
    };
};

// Optional than can support chained val calls given that default constructor is valid.
// Useful to descend into LSP structures.