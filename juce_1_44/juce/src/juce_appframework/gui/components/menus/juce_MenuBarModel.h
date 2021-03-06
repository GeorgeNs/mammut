/*
  ==============================================================================

   This file is part of the JUCE library - "Jules' Utility Class Extensions"
   Copyright 2004-7 by Raw Material Software ltd.

  ------------------------------------------------------------------------------

   JUCE can be redistributed and/or modified under the terms of the
   GNU General Public License, as published by the Free Software Foundation;
   either version 2 of the License, or (at your option) any later version.

   JUCE is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with JUCE; if not, visit www.gnu.org/licenses or write to the
   Free Software Foundation, Inc., 59 Temple Place, Suite 330,
   Boston, MA 02111-1307 USA

  ------------------------------------------------------------------------------

   If you'd like to release a closed-source product which uses JUCE, commercial
   licenses are also available: visit www.rawmaterialsoftware.com/juce for
   more information.

  ==============================================================================
*/

#ifndef __JUCE_MENUBARMODEL_JUCEHEADER__
#define __JUCE_MENUBARMODEL_JUCEHEADER__

#include "juce_PopupMenu.h"
class MenuBarModel;


//==============================================================================
/**
    A class to receive callbacks when a MenuBarModel changes.

    @see MenuBarModel::addListener, MenuBarModel::removeListener, MenuBarModel::menuItemsChanged
*/
class JUCE_API  MenuBarModelListener
{
public:
    /** Destructor. */
    virtual ~MenuBarModelListener() {}

    //==============================================================================
    /** This callback is made when items are changed in the menu bar model.
    */
    virtual void menuBarItemsChanged (MenuBarModel* menuBarModel) = 0;

    /** This callback is made when an application command is invoked that
        is represented by one of the items in the menu bar model.
    */
    virtual void menuCommandInvoked (MenuBarModel* menuBarModel,
                                     const ApplicationCommandTarget::InvocationInfo& info) = 0;
};


//==============================================================================
/**
    A class for controlling MenuBar components.

    This class is used to tell a MenuBar what menus to show, and to respond
    to a menu being selected.

    @see MenuBarModelListener, MenuBarComponent, PopupMenu
*/
class JUCE_API  MenuBarModel      : private AsyncUpdater,
                                    private ApplicationCommandManagerListener
{
public:
    //==============================================================================
    MenuBarModel() throw();

    /** Destructor. */
    virtual ~MenuBarModel();

    //==============================================================================
    /** Call this when some of your menu items have changed.

        This method will cause a callback to any MenuBarListener objects that
        are registered with this model.

        If this model is displaying items from an ApplicationCommandManager, you
        can use the setApplicationCommandManagerToWatch() method to cause
        change messages to be sent automatically when the ApplicationCommandManager
        is changed.

        @see addListener, removeListener, MenuBarListener
    */
    void menuItemsChanged();

    /** Tells the menu bar to listen to the specified command manager, and to update
        itself when the commands change.

        This will also allow it to flash a menu name when a command from that menu
        is invoked using a keystroke.
    */
    void setApplicationCommandManagerToWatch (ApplicationCommandManager* const manager) throw();

    /** Registers a listener for callbacks when the menu items in this model change.

        The listener object will get callbacks when this object's menuItemsChanged()
        method is called.

        @see removeListener
    */
    void addListener (MenuBarModelListener* const listenerToAdd) throw();

    /** Removes a listener.

        @see addListener
    */
    void removeListener (MenuBarModelListener* const listenerToRemove) throw();

    //==============================================================================
    /** This method must return a list of the names of the menus. */
    virtual const StringArray getMenuBarNames() = 0;

    /** This should return the popup menu to display for a given top-level menu.

        @param topLevelMenuIndex    the index of the top-level menu to show
        @param menuName             the name of the top-level menu item to show
    */
    virtual const PopupMenu getMenuForIndex (int topLevelMenuIndex,
                                             const String& menuName) = 0;

    /** This is called when a menu item has been clicked on.

        @param menuItemID           the item ID of the PopupMenu item that was selected
        @param topLevelMenuIndex    the index of the top-level menu from which the item was
                                    chosen (just in case you've used duplicate ID numbers
                                    on more than one of the popup menus)
    */
    virtual void menuItemSelected (int menuItemID,
                                   int topLevelMenuIndex) = 0;

    //==============================================================================
#if JUCE_MAC || DOXYGEN
    /** MAC ONLY - Sets the model that is currently being shown as the main
        menu bar at the top of the screen on the Mac.

        You can pass 0 to stop the current model being displayed. Be careful
        not to delete a model while it is being used.
    */
    static void setMacMainMenu (MenuBarModel* newMenuBarModel) throw();

    /** MAC ONLY - Returns the menu model that is currently being shown as
        the main menu bar.
    */
    static MenuBarModel* getMacMainMenu() throw();
#endif

    //==============================================================================
    /** @internal */
    void applicationCommandInvoked (const ApplicationCommandTarget::InvocationInfo& info);
    /** @internal */
    void applicationCommandListChanged();
    /** @internal */
    void handleAsyncUpdate();

    juce_UseDebuggingNewOperator

private:
    ApplicationCommandManager* manager;
    SortedSet <void*> listeners;

    MenuBarModel (const MenuBarModel&);
    const MenuBarModel& operator= (const MenuBarModel&);
};


#endif   // __JUCE_MENUBARMODEL_JUCEHEADER__
