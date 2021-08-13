namespace std {
    class type_info {
      public:
        virtual ~type_info() {};
        bool operator==(const type_info &) const;
        bool operator!=(const type_info &) const;
        bool before(const type_info &) const;
        const char* name() const;

      private:
        type_info (const type_info& rhs);
        type_info& operator= (const type_info& rhs);

        const char *__type_name;
    };
}

namespace __cxxabiv1 {

class __class_type_info : public std::type_info
{
public:
    virtual ~__class_type_info();
};

__class_type_info::~__class_type_info() {}

class __si_class_type_info : public __class_type_info
{
public:
    const __class_type_info *__base_type;
    virtual ~__si_class_type_info() override;
};

__si_class_type_info::~__si_class_type_info() {}

}
