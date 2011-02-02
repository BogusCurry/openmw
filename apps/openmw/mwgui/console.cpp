
#include "console.hpp"

#include <algorithm> 

#include <iostream>

#include <components/compiler/exception.hpp>

#include "../mwscript/extensions.hpp"

namespace MWGui
{
    class ConsoleInterpreterContext : public MWScript::InterpreterContext
    {
            Console& mConsole;

        public:

            ConsoleInterpreterContext (Console& console, MWWorld::Environment& environment,
                MWWorld::Ptr reference);
    };

    ConsoleInterpreterContext::ConsoleInterpreterContext (Console& console,
        MWWorld::Environment& environment, MWWorld::Ptr reference)
    : MWScript::InterpreterContext (environment,
        reference.isEmpty() ? 0 : &reference.getRefData().getLocals(), reference),
      mConsole (console)
    {}

    bool Console::compile (const std::string& cmd, Compiler::Output& output)
    {

  listNames();
  for (std::vector<std::string>::iterator iter (mNames.begin()); iter!=mNames.end(); ++iter)
    std::cout << *iter << ", ";
  std::cout << std::endl;

        try
        {
            ErrorHandler::reset();

            std::istringstream input (cmd + '\n');

            Compiler::Scanner scanner (*this, input, mCompilerContext.getExtensions());

            Compiler::LineParser parser (*this, mCompilerContext, output.getLocals(),
                output.getLiterals(), output.getCode(), true);

            scanner.scan (parser);

            return isGood();
        }
        catch (const Compiler::SourceException& error)
        {
            // error has already been reported via error handler
        }
        catch (const std::exception& error)
        {
            printError (std::string ("An exception has been thrown: ") + error.what());
        }

        return false;
    }

    void Console::report (const std::string& message, const Compiler::TokenLoc& loc, Type type)
    {
        std::ostringstream error;
        error << "column " << loc.mColumn << " (" << loc.mLiteral << "):";

        printError (error.str());
        printError ((type==ErrorMessage ? "error: " : "warning: ") + message);
    }

    void Console::report (const std::string& message, Type type)
    {
        printError ((type==ErrorMessage ? "error: " : "warning: ") + message);
    }

    void Console::listNames()
    {
        if (mNames.empty())
        {
            // keywords
            std::istringstream input ("");

            Compiler::Scanner scanner (*this, input, mCompilerContext.getExtensions());

            scanner.listKeywords (mNames);

            // identifier
            const ESMS::ESMStore& store = mEnvironment.mWorld->getStore();

            for (ESMS::RecListList::const_iterator iter (store.recLists.begin());
                iter!=store.recLists.end(); ++iter)
            {
                iter->second->listIdentifier (mNames);
            }

            // sort
            std::sort (mNames.begin(), mNames.end());
        }
    }

    Console::Console(int w, int h, MWWorld::Environment& environment,
        const Compiler::Extensions& extensions)
      : Layout("openmw_console_layout.xml"),
        mCompilerContext (MWScript::CompilerContext::Type_Console, environment),
        mEnvironment (environment)
    {

        setCoord(10,10, w-10, h/2);

        getWidget(command, "edit_Command");
        getWidget(history, "list_History");

        // Set up the command line box
        command->eventEditSelectAccept =
            newDelegate(this, &Console::acceptCommand);
        command->eventKeyButtonPressed =
            newDelegate(this, &Console::keyPress);

        // Set up the log window
        history->setOverflowToTheLeft(true);
        history->setEditStatic(true);
        history->setVisibleVScroll(true);

        // compiler
        mCompilerContext.setExtensions (&extensions);
        listNames(); // We can have tab_completion before the first call to Console::compile
    }

    void Console::enable()
    {
        setVisible(true);

        // Give keyboard focus to the combo box whenever the console is
        // turned on
        MyGUI::InputManager::getInstance().setKeyFocusWidget(command);
    }

    void Console::disable()
    {
        setVisible(false);
    }

    void Console::setFont(const std::string &fntName)
    {
        history->setFontName(fntName);
        command->setFontName(fntName);
    }

    void Console::clearHistory()
    {
        history->setCaption("");
    }

    void Console::print(const std::string &msg)
    {
        history->addText(msg);
    }

    void Console::printOK(const std::string &msg)
    {
        print("#FF00FF" + msg + "\n");
    }

    void Console::printError(const std::string &msg)
    {
        print("#FF2222" + msg + "\n");
    }

    std::vector<std::string> Console::delimitString(std::string string, char delimit)
    {
        std::vector<std::string> delimitedStrings;
        size_t lastIndex = 0;
        size_t curIndex = string.find(delimit, 0); // current Index into 'string'
        delimitedStrings.push_back(string.substr(lastIndex, curIndex));

        while(curIndex != std::string::npos and lastIndex != std::string::npos)
        {
            curIndex = string.find(delimit, lastIndex);
            delimitedStrings.push_back(string.substr(lastIndex, curIndex - 1));
            lastIndex = curIndex + 1;
        }

        if(delimitedStrings.empty())
            delimitedStrings.push_back(string);

        return delimitedStrings;
    }

    void Console::keyPress(MyGUI::WidgetPtr _sender,
                  MyGUI::KeyCode key,
                  MyGUI::Char _char)
    {
        if(key == MyGUI::KeyCode::Tab)
        {
            editString = command->getCaption();

           // Find the last element
            if(editString.find('"', 0) != std::string::npos)
            {
                size_t quote = editString.find('"', 0);
                size_t oldquote;

                while(quote != std::string::npos) 
                {
                    oldquote = quote;
                    quote = editString.find('"', quote + 1);
                    if(quote == std::string::npos)
                    {
                        editString = editString.substr(oldquote + 1, quote);
                    }
                }
            }

            else
            {
                // Split editString into an vector delimited by spaces.
                std::vector<std::string> strings = delimitString(editString, ' ');
                editString = strings.back();
            }

            std::vector<std::string> matches;
            std::vector<std::string>::iterator mNames_iter = mNames.begin();

            while(mNames_iter != mNames.end())
            {
                size_t curMatch = (*mNames_iter).find(editString);
                if(curMatch != std::string::npos)
                    matches.push_back((*mNames_iter));
                mNames_iter++;
            }

            std::vector<std::string>::iterator iter = matches.begin();

            if(!matches.empty())
            {
                printOK("Matches: ");
                if(matches.size() <= 1)
                    printOK(*iter);

                while(iter != matches.end() && matches.size() > 1)
                {
                    printOK(*iter);
                    iter++;
                }
            }
        }
 
        if(command_history.empty()) return;

        // Traverse history with up and down arrows
        if(key == MyGUI::KeyCode::ArrowUp)
        {
            // If the user was editing a string, store it for later
            if(current == command_history.end())
                editString = command->getCaption();

            if(current != command_history.begin())
            {
                current--;
                command->setCaption(*current);
            }
        }
        else if(key == MyGUI::KeyCode::ArrowDown)
        {
            if(current != command_history.end())
            {
                current++;

                if(current != command_history.end())
                    command->setCaption(*current);
                else
                    // Restore the edit string
                    command->setCaption(editString);
            }
        }
   }

    void Console::acceptCommand(MyGUI::EditPtr _sender)
    {
        const std::string &cm = command->getCaption();
        if(cm.empty()) return;

        // Add the command to the history, and set the current pointer to
        // the end of the list
        command_history.push_back(cm);
        current = command_history.end();
        editString.clear();

        // Log the command
        print("#FFFFFF> " + cm + "\n");

        Compiler::Locals locals;
        Compiler::Output output (locals);

        if (compile (cm + "\n", output))
        {
            try
            {
                ConsoleInterpreterContext interpreterContext (*this, mEnvironment, MWWorld::Ptr());
                Interpreter::Interpreter interpreter (interpreterContext);
                MWScript::installOpcodes (interpreter);
                std::vector<Interpreter::Type_Code> code;
                output.getCode (code);
                interpreter.run (&code[0], code.size());
            }
            catch (const std::exception& error)
            {
                printError (std::string ("An exception has been thrown: ") + error.what());
            }
        }

        command->setCaption("");
    }
}
