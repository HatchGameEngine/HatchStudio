#pragma once

#include <UI/Controls/Form.hpp>

namespace UI::System::Application {
    extern Form* BaseForm;
    extern bool CancelShortcuts;

    void Start(int argc, char** args, Form* startForm);
    void ShowDialog(Form* dialog, DialogCallback callback);
    void Show(Form* form);

    extern char* optarg;
    int ParseOptions(int nargc, char* const nargv[], const char* options);
}
