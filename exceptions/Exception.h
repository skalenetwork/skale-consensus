#pragma  once



class Exception: public std::exception, public boost::exception
{

public:

    Exception(const string& _message, const string& _className) {
        message = _className + ":" + _message;

    }
    const char* what() const noexcept override { return message.empty() ? std::exception::what() : message.c_str(); }

    const std::string &getMessage() const {
        return message;
    }

    bool isFatal() const {
        return fatal;
    }

private:
    std::string message;

protected:

    bool fatal = false;

public:

    static void log_exception(const std::exception& e, int level =  0);


};