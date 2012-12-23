/*
  ==============================================================================

   This file is part of the JUCE library - "Jules' Utility Class Extensions"
   Copyright 2004-11 by Raw Material Software Ltd.

  ------------------------------------------------------------------------------

   JUCE can be redistributed and/or modified under the terms of the GNU General
   Public License (Version 2), as published by the Free Software Foundation.
   A copy of the license is included in the JUCE distribution, or can be found
   online at www.gnu.org/licenses.

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.rawmaterialsoftware.com/juce for more information.

  ==============================================================================
*/

class TableListBox::RowComp   : public Component,
                                public TooltipClient
{
public:
    RowComp (TableListBox& tlb)  : owner (tlb), row (-1), isSelected (false)
    {
    }

    void paint (Graphics& g)
    {
        if (TableListBoxModel* const tableModel = owner.getModel())
        {
            tableModel->paintRowBackground (g, row, getWidth(), getHeight(), isSelected);

            const TableHeaderComponent& headerComp = owner.getHeader();
            const int numColumns = headerComp.getNumColumns (true);

            for (int i = 0; i < numColumns; ++i)
            {
                if (columnComponents[i] == nullptr)
                {
                    const int columnId = headerComp.getColumnIdOfIndex (i, true);
                    const Rectangle<int> columnRect (headerComp.getColumnPosition(i).withHeight (getHeight()));

                    Graphics::ScopedSaveState ss (g);

                    g.reduceClipRegion (columnRect);
                    g.setOrigin (columnRect.getX(), 0);
                    tableModel->paintCell (g, row, columnId, columnRect.getWidth(), columnRect.getHeight(), isSelected);
                }
            }
        }
    }

    void update (const int newRow, const bool isNowSelected)
    {
        jassert (newRow >= 0);

        if (newRow != row || isNowSelected != isSelected)
        {
            row = newRow;
            isSelected = isNowSelected;
            repaint();
        }

        TableListBoxModel* const tableModel = owner.getModel();

        if (tableModel != nullptr && row < owner.getNumRows())
        {
            const Identifier columnProperty ("_tableColumnId");
            const int numColumns = owner.getHeader().getNumColumns (true);

            for (int i = 0; i < numColumns; ++i)
            {
                const int columnId = owner.getHeader().getColumnIdOfIndex (i, true);
                Component* comp = columnComponents[i];

                if (comp != nullptr && columnId != (int) comp->getProperties() [columnProperty])
                {
                    columnComponents.set (i, nullptr);
                    comp = nullptr;
                }

                comp = tableModel->refreshComponentForCell (row, columnId, isSelected, comp);
                columnComponents.set (i, comp, false);

                if (comp != nullptr)
                {
                    comp->getProperties().set (columnProperty, columnId);

                    addAndMakeVisible (comp);
                    resizeCustomComp (i);
                }
            }

            columnComponents.removeRange (numColumns, columnComponents.size());
        }
        else
        {
            columnComponents.clear();
        }
    }

    void resized()
    {
        for (int i = columnComponents.size(); --i >= 0;)
            resizeCustomComp (i);
    }

    void resizeCustomComp (const int index)
    {
        if (Component* const c = columnComponents.getUnchecked (index))
            c->setBounds (owner.getHeader().getColumnPosition (index)
                            .withY (0).withHeight (getHeight()));
    }

    void mouseDown (const MouseEvent& e)
    {
        isDragging = false;
        selectRowOnMouseUp = false;

        if (isEnabled())
        {
            if (! isSelected)
            {
                owner.selectRowsBasedOnModifierKeys (row, e.mods, false);

                const int columnId = owner.getHeader().getColumnIdAtX (e.x);

                if (columnId != 0)
                    if (TableListBoxModel* model = owner.getModel())
                        model->cellClicked (row, columnId, e);
            }
            else
            {
                selectRowOnMouseUp = true;
            }
        }
    }

    void mouseDrag (const MouseEvent& e)
    {
        if (isEnabled() && owner.getModel() != nullptr && ! (e.mouseWasClicked() || isDragging))
        {
            const SparseSet<int> selectedRows (owner.getSelectedRows());

            if (selectedRows.size() > 0)
            {
                const var dragDescription (owner.getModel()->getDragSourceDescription (selectedRows));

                if (! (dragDescription.isVoid() || (dragDescription.isString() && dragDescription.toString().isEmpty())))
                {
                    isDragging = true;
                    owner.startDragAndDrop (e, dragDescription, true);
                }
            }
        }
    }

    void mouseUp (const MouseEvent& e)
    {
        if (selectRowOnMouseUp && e.mouseWasClicked() && isEnabled())
        {
            owner.selectRowsBasedOnModifierKeys (row, e.mods, true);

            const int columnId = owner.getHeader().getColumnIdAtX (e.x);

            if (columnId != 0)
                if (TableListBoxModel* model = owner.getModel())
                    model->cellClicked (row, columnId, e);
        }
    }

    void mouseDoubleClick (const MouseEvent& e)
    {
        const int columnId = owner.getHeader().getColumnIdAtX (e.x);

        if (columnId != 0)
            if (TableListBoxModel* model = owner.getModel())
                model->cellDoubleClicked (row, columnId, e);
    }

    String getTooltip()
    {
        const int columnId = owner.getHeader().getColumnIdAtX (getMouseXYRelative().getX());

        if (columnId != 0)
            if (TableListBoxModel* model = owner.getModel())
                return model->getCellTooltip (row, columnId);

        return String::empty;
    }

    Component* findChildComponentForColumn (const int columnId) const
    {
        return columnComponents [owner.getHeader().getIndexOfColumnId (columnId, true)];
    }

private:
    TableListBox& owner;
    OwnedArray<Component> columnComponents;
    int row;
    bool isSelected, isDragging, selectRowOnMouseUp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RowComp)
};


//==============================================================================
class TableListBox::Header  : public TableHeaderComponent
{
public:
    Header (TableListBox& tlb)  : owner (tlb) {}

    void addMenuItems (PopupMenu& menu, int columnIdClicked)
    {
        if (owner.isAutoSizeMenuOptionShown())
        {
            menu.addItem (autoSizeColumnId, TRANS("Auto-size this column"), columnIdClicked != 0);
            menu.addItem (autoSizeAllId, TRANS("Auto-size all columns"), owner.getHeader().getNumColumns (true) > 0);
            menu.addSeparator();
        }

        TableHeaderComponent::addMenuItems (menu, columnIdClicked);
    }

    void reactToMenuItem (int menuReturnId, int columnIdClicked)
    {
        switch (menuReturnId)
        {
            case autoSizeColumnId:      owner.autoSizeColumn (columnIdClicked); break;
            case autoSizeAllId:         owner.autoSizeAllColumns(); break;
            default:                    TableHeaderComponent::reactToMenuItem (menuReturnId, columnIdClicked); break;
        }
    }

private:
    TableListBox& owner;

    enum { autoSizeColumnId = 0xf836743, autoSizeAllId = 0xf836744 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Header)
};

//==============================================================================
TableListBox::TableListBox (const String& name, TableListBoxModel* const m)
    : ListBox (name, nullptr),
      header (nullptr),
      model (m),
      autoSizeOptionsShown (true)
{
    ListBox::model = this;

    setHeader (new Header (*this));
}

TableListBox::~TableListBox()
{
}

void TableListBox::setModel (TableListBoxModel* const newModel)
{
    if (model != newModel)
    {
        model = newModel;
        updateContent();
    }
}

void TableListBox::setHeader (TableHeaderComponent* newHeader)
{
    jassert (newHeader != nullptr); // you need to supply a real header for a table!

    Rectangle<int> newBounds (0, 0, 100, 28);
    if (header != nullptr)
        newBounds = header->getBounds();

    header = newHeader;
    header->setBounds (newBounds);

    setHeaderComponent (header);

    header->addListener (this);
}

int TableListBox::getHeaderHeight() const
{
    return header->getHeight();
}

void TableListBox::setHeaderHeight (const int newHeight)
{
    header->setSize (header->getWidth(), newHeight);
    resized();
}

void TableListBox::autoSizeColumn (const int columnId)
{
    const int width = model != nullptr ? model->getColumnAutoSizeWidth (columnId) : 0;

    if (width > 0)
        header->setColumnWidth (columnId, width);
}

void TableListBox::autoSizeAllColumns()
{
    for (int i = 0; i < header->getNumColumns (true); ++i)
        autoSizeColumn (header->getColumnIdOfIndex (i, true));
}

void TableListBox::setAutoSizeMenuOptionShown (const bool shouldBeShown)
{
    autoSizeOptionsShown = shouldBeShown;
}

bool TableListBox::isAutoSizeMenuOptionShown() const
{
    return autoSizeOptionsShown;
}

Rectangle<int> TableListBox::getCellPosition (const int columnId, const int rowNumber,
                                              const bool relativeToComponentTopLeft) const
{
    Rectangle<int> headerCell (header->getColumnPosition (header->getIndexOfColumnId (columnId, true)));

    if (relativeToComponentTopLeft)
        headerCell.translate (header->getX(), 0);

    return getRowPosition (rowNumber, relativeToComponentTopLeft)
            .withX (headerCell.getX())
            .withWidth (headerCell.getWidth());
}

Component* TableListBox::getCellComponent (int columnId, int rowNumber) const
{
    if (RowComp* const rowComp = dynamic_cast <RowComp*> (getComponentForRowNumber (rowNumber)))
        return rowComp->findChildComponentForColumn (columnId);

    return nullptr;
}

void TableListBox::scrollToEnsureColumnIsOnscreen (const int columnId)
{
    if (ScrollBar* const scrollbar = getHorizontalScrollBar())
    {
        const Rectangle<int> pos (header->getColumnPosition (header->getIndexOfColumnId (columnId, true)));

        double x = scrollbar->getCurrentRangeStart();
        const double w = scrollbar->getCurrentRangeSize();

        if (pos.getX() < x)
            x = pos.getX();
        else if (pos.getRight() > x + w)
            x += jmax (0.0, pos.getRight() - (x + w));

        scrollbar->setCurrentRangeStart (x);
    }
}

int TableListBox::getNumRows()
{
    return model != nullptr ? model->getNumRows() : 0;
}

void TableListBox::paintListBoxItem (int, Graphics&, int, int, bool)
{
}

Component* TableListBox::refreshComponentForRow (int rowNumber, bool isRowSelected_, Component* existingComponentToUpdate)
{
    if (existingComponentToUpdate == nullptr)
        existingComponentToUpdate = new RowComp (*this);

    static_cast <RowComp*> (existingComponentToUpdate)->update (rowNumber, isRowSelected_);

    return existingComponentToUpdate;
}

void TableListBox::selectedRowsChanged (int row)
{
    if (model != nullptr)
        model->selectedRowsChanged (row);
}

void TableListBox::deleteKeyPressed (int row)
{
    if (model != nullptr)
        model->deleteKeyPressed (row);
}

void TableListBox::returnKeyPressed (int row)
{
    if (model != nullptr)
        model->returnKeyPressed (row);
}

void TableListBox::backgroundClicked()
{
    if (model != nullptr)
        model->backgroundClicked();
}

void TableListBox::listWasScrolled()
{
    if (model != nullptr)
        model->listWasScrolled();
}

void TableListBox::tableColumnsChanged (TableHeaderComponent*)
{
    setMinimumContentWidth (header->getTotalWidth());
    repaint();
    updateColumnComponents();
}

void TableListBox::tableColumnsResized (TableHeaderComponent*)
{
    setMinimumContentWidth (header->getTotalWidth());
    repaint();
    updateColumnComponents();
}

void TableListBox::tableSortOrderChanged (TableHeaderComponent*)
{
    if (model != nullptr)
        model->sortOrderChanged (header->getSortColumnId(),
                                 header->isSortedForwards());
}

void TableListBox::tableColumnDraggingChanged (TableHeaderComponent*, int columnIdNowBeingDragged_)
{
    columnIdNowBeingDragged = columnIdNowBeingDragged_;
    repaint();
}

void TableListBox::resized()
{
    ListBox::resized();

    header->resizeAllColumnsToFit (getVisibleContentWidth());
    setMinimumContentWidth (header->getTotalWidth());
}

void TableListBox::updateColumnComponents() const
{
    const int firstRow = getRowContainingPosition (0, 0);

    for (int i = firstRow + getNumRowsOnScreen() + 2; --i >= firstRow;)
        if (RowComp* const rowComp = dynamic_cast <RowComp*> (getComponentForRowNumber (i)))
            rowComp->resized();
}

//==============================================================================
void TableListBoxModel::cellClicked (int, int, const MouseEvent&)       {}
void TableListBoxModel::cellDoubleClicked (int, int, const MouseEvent&) {}
void TableListBoxModel::backgroundClicked()                             {}
void TableListBoxModel::sortOrderChanged (int, const bool)              {}
int TableListBoxModel::getColumnAutoSizeWidth (int)                     { return 0; }
void TableListBoxModel::selectedRowsChanged (int)                       {}
void TableListBoxModel::deleteKeyPressed (int)                          {}
void TableListBoxModel::returnKeyPressed (int)                          {}
void TableListBoxModel::listWasScrolled()                               {}

String TableListBoxModel::getCellTooltip (int /*rowNumber*/, int /*columnId*/)    { return String::empty; }
var TableListBoxModel::getDragSourceDescription (const SparseSet<int>&)           { return var::null; }

Component* TableListBoxModel::refreshComponentForCell (int, int, bool, Component* existingComponentToUpdate)
{
    (void) existingComponentToUpdate;
    jassert (existingComponentToUpdate == nullptr); // indicates a failure in the code the recycles the components
    return nullptr;
}
