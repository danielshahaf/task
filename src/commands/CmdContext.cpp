////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2006 - 2015, Paul Beckingham, Federico Hernandez.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// http://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

#include <cmake.h>
#include <Context.h>
#include <sstream>
#include <algorithm>
#include <i18n.h>
#include <text.h>
#include <CmdContext.h>
#include <CmdConfig.h>

extern Context context;

////////////////////////////////////////////////////////////////////////////////
CmdContext::CmdContext ()
{
  _keyword     = "context";
  _usage       = "task          context [<name> | subcommand]";
  _description = STRING_CMD_CONTEXT_USAGE;
  _read_only   = true;
  _displays_id = false;
}

////////////////////////////////////////////////////////////////////////////////
int CmdContext::execute (std::string& output)
{
  int rc = 0;
  std::stringstream out;

  // Get the non-attribute, non-fancy command line arguments.
  std::vector <std::string> words = context.cli.getWords ();

  if (words.size () > 0)
  {
    std::string subcommand = words[0];

    if (subcommand == "define")
      rc = defineContext (words, out);
    else if (subcommand == "delete")
      rc = deleteContext (words, out);
    else if (subcommand == "list")
      rc = listContexts (words, out);
    else if (subcommand == "none")
      rc = unsetContext (words, out);
    else if (subcommand == "show")
      rc = showContext (words, out);
    else
      rc = setContext (words, out);
  }

  output = out.str ();
  return rc;
}

////////////////////////////////////////////////////////////////////////////////
// Joins all the words in the specified interval <from, to) to one string,
// which is then returned.
//
// If to is specified as 0 (default value), all the remaining words will be joined.
//
std::string CmdContext::joinWords (std::vector <std::string>& words, unsigned int from, unsigned int to /* = 0 */)
{
  std::string value = "";

  if (to == 0)
    to = words.size();

  for (unsigned int i = from; i < to; ++i)
  {
    if (i > from)
      value += " ";

    value += words[i];
  }

  return value;
}

////////////////////////////////////////////////////////////////////////////////
// Returns all user defined contexts.
//
std::vector <std::string> CmdContext::getContexts ()
{
  std::vector <std::string> contexts;

  Config::const_iterator name;
  for (name = context.config.begin (); name != context.config.end (); ++name)
    if (name->first.substr (0, 8) == "context.")
      contexts.push_back (name->first.substr (8));

  return contexts;
}

////////////////////////////////////////////////////////////////////////////////
// Defines a new user-provided context.
//  - The context definition is written into .taskrc as a context.<name> variable.
//  - Deletion of the context requires confirmation if rc.confirmation=yes.
//
// Returns: 0 if the addition of the config variable was successful, 1 otherwise
//
// Invoked with: task context define <name> <filter>
// Example:      task context define home project:Home
//
int CmdContext::defineContext (std::vector <std::string>& words, std::stringstream& out)
{
  int rc = 0;

  if (words.size () > 2)
  {
    std::string name = "context." + words[1];
    std::string value = joinWords(words, 2);
    // TODO: Check if the value is a proper filter

    // Set context definition config variable
    bool confirmation = context.config.getBoolean ("confirmation");
    bool success = CmdConfig::setConfigVariable(name, value, confirmation);

    if (success)
      out << "Context '" << words[1] << "' defined." << "\n";
    else
    {
      out << "Context '" << words[1] << "' was not defined." << "\n";
      rc = 1;
    }
  }
  else
    throw "Both context name and its definition must be provided.";

  return rc;
}

////////////////////////////////////////////////////////////////////////////////
// Deletes the specified context.
//  - If the deleted context is currently active, unset it.
//  - Deletion of the context requires confirmation if rc.confirmation=yes.
//
// Returns: 0 if the removal of the config variable was successful, 1 otherwise
//
// Invoked with: task context delete <name>
// Example:      task context delete home
//
int CmdContext::deleteContext (std::vector <std::string>& words, std::stringstream& out)
{
  int rc = 0;

  if (words.size () > 1)
  {
    // Delete the specified context
    std::string name = "context." + words[1];

    bool confirmation = context.config.getBoolean ("confirmation");
    rc = CmdConfig::unsetConfigVariable(name, confirmation);

    // If the currently set context was deleted, unset it
    std::string currentContext = context.config.get ("context");

    if (currentContext == words[1])
      CmdConfig::unsetConfigVariable("context", false);

    // Output feedback
    if (rc == 0)
      out << "Context '" << words[1] << "' undefined." << "\n";
    else
      out << "Context '" << words[1] << "' was not undefined." << "\n";
  }
  else
    throw "Context name needs to be specified.";

  return rc;
}

////////////////////////////////////////////////////////////////////////////////
// Render a list of context names and their definitions.
//
// Returns: 0 the resulting list is non-empty, 1 otherwise
//
// Invoked with: task context list
// Example:      task context list
//
int CmdContext::listContexts (std::vector <std::string>& words, std::stringstream& out)
{
  int rc = 0;
  std::vector <std::string> contexts = getContexts();

  if (contexts.size ())
  {
    std::sort (contexts.begin (), contexts.end ());

    ViewText view;
    view.width (context.getWidth ());
    view.add (Column::factory ("string", "Name"));
    view.add (Column::factory ("string", "Definition"));

    Color label (context.config.get ("color.label"));
    view.colorHeader (label);

    std::vector <std::string>::iterator userContext;
    for (userContext = contexts.begin (); userContext != contexts.end (); ++userContext)
    {
      std::string definition = context.config.get ("context." + *userContext);

      int row = view.addRow ();
      view.set (row, 0, *userContext);
      view.set (row, 1, definition);
    }

    out << optionalBlankLine ()
        << view.render ()
        << optionalBlankLine ();
  }
  else
  {
    out << "No contexts defined." << "\n";
    rc = 1;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////
// Sets the specified context as currently active.
//   - If some other context was active, the value of currently active context
//     is replaced, not added.
//   - Setting of the context does not require confirmation.
//
// Returns: 0 if the setting of the context was successful, 1 otherwise
//
// Invoked with: task context <name>
// Example:      task context home
//
int CmdContext::setContext (std::vector <std::string>& words, std::stringstream& out)
{
  int rc = 0;
  std::string value = words[0];
  std::vector <std::string> contexts = getContexts ();

  // Check that the specified context is defined
  if (std::find (contexts.begin (), contexts.end (), value) == contexts.end())
    throw format ("Context '{1}' not found.", value);

  // Set the active context.
  // Should always succeed, as we do not require confirmation.
  bool success = CmdConfig::setConfigVariable("context", value, false);

  if (success)
    out << "Context '" << value << "' applied." << "\n";
  else
  {
    out << "Context '" << value << "' was not applied." << "\n";
    rc = 1;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////
// Shows the currently active context.
//
// Returns: Always returns 0.
//
// Invoked with: task context show
// Example:      task context show
//
int CmdContext::showContext (std::vector <std::string>& words, std::stringstream& out)
{
  std::string currentContext = context.config.get ("context");

  if (currentContext == "")
    out << "No context is currently applied." << "\n";
  else
  {
    std::string currentFilter = context.config.get ("context." + currentContext);
    out << format ("Context '{1}' with filter '{2}' is currently applied.", currentContext, currentFilter) << "\n";
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Unsets the currently active context.
//   - Unsetting of the context does not require confirmation.
//
// Returns: 0 if the unsetting of the context was successful, 1 otherwise (also
//          returned if no context is currently active)
//
// Invoked with: task context none
// Example:      task context none
//
int CmdContext::unsetContext (std::vector <std::string>& words, std::stringstream& out)
{
  int rc = 0;
  int status = CmdConfig::unsetConfigVariable("context", false);

  if (status == 0)
    out << "Context unset." << "\n";
  else
  {
    out << "Context not unset." << "\n";
    rc = 1;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////
CmdCompletionContext::CmdCompletionContext ()
{
  _keyword     = "_context";
  _usage       = "task          _context";
  _description = STRING_CMD_HCONTEXT_USAGE;
  _read_only   = true;
  _displays_id = false;
}

////////////////////////////////////////////////////////////////////////////////
int CmdCompletionContext::execute (std::string& output)
{
  std::vector <std::string> userContexts = CmdContext::getContexts ();

  std::vector <std::string>::iterator userContext;
  for (userContext = userContexts.begin (); userContext != userContexts.end (); ++userContext)
    output += *userContext + "\n";

  return 0;
}

////////////////////////////////////////////////////////////////////////////////