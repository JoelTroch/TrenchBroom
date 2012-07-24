/*
 GWEN
 Copyright (c) 2010 Facepunch Studios
 See license in Gwen.h
 */


#include "Gwen/Gwen.h"
#include "Gwen/Skin.h"
#include "Gwen/Controls/Properties.h"
#include "Gwen/Utility.h"

#include <algorithm>
#include <vector>

namespace Gwen
{
    namespace Controls
    {
        
        GWEN_CONTROL_CONSTRUCTOR( Properties )
        {
            m_SplitterBar = new SplitterBar( this );
            m_SplitterBar->SetPos( 80, 0 );
            m_SplitterBar->SetCursor( Gwen::CursorType::SizeWE );
            m_SplitterBar->onDragged.Add( this, &Properties::OnSplitterMoved );
            m_SplitterBar->SetShouldDrawBackground( false );
            m_sorted = false;
            m_emptyRow = NULL;
            m_formerEmptyRow = NULL;
        }
        
        bool Properties::CompareControls::operator() (Gwen::Controls::Base* first, Gwen::Controls::Base* second) {
            PropertyRow* firstRow = gwen_cast<PropertyRow>(first);
            PropertyRow* secondRow = gwen_cast<PropertyRow>(second);
            
            if (firstRow == NULL && secondRow == NULL)
                return false;
            
            if (firstRow == NULL)
                return false;
            
            if (secondRow == NULL)
                return true;
            
            // empty rows go to the bottom
            if (firstRow->GetKey()->GetContent() == L"")
                return false;
            
            if (secondRow->GetKey()->GetContent() == L"")
                return true;
            
            return firstRow->GetKey()->GetContent().compare(secondRow->GetKey()->GetContent()) <= 0;
        }
        
        Base::List Properties::GetChildrenForLayout()
        {
            if (Children.empty())
                return Children;
            
            std::vector<Gwen::Controls::Base*> SortedChildren;
            for (Base::List::iterator it = Children.begin(); it != Children.end(); ++it)
                if (*it != m_emptyRow)
                    SortedChildren.push_back(*it);
            if (m_sorted) {
                CompareControls compare;
                std::sort(SortedChildren.begin(), SortedChildren.end(), compare);
            }
            
            if (m_emptyRow != NULL)
                SortedChildren.push_back(m_emptyRow);
            
            Base::List Result;
            Result.insert(Result.begin(), SortedChildren.begin(), SortedChildren.end());
            return Result;
        }
        
        void Properties::PostLayout( Gwen::Skin::Base* /*skin*/ )
        {
            m_SplitterBar->SetHeight( Height() );
            
            if ( SizeToChildren( false, true ) )
            {
                InvalidateParent();
            }
            
            m_SplitterBar->SetSize( 3, Height() );
        }
        
        void Properties::OnSplitterMoved( Controls::Base * /*control*/ )
        {
            InvalidateChildren();
        }
        
        int Properties::GetSplitWidth()
        {
            return m_SplitterBar->X();
        }
        
        PropertyRow* Properties::Add( const TextObject& key, const TextObject& value )
        {
            Property::Text* valueProp = new Property::Text(this);
            valueProp->SetContent(value, false);
            return Add( key, valueProp, value );
        }
        
        PropertyRow* Properties::Add( const TextObject& key, Property::Base* pProp, const TextObject& value )
        {
            PropertyRow* row = new PropertyRow( this );
            row->Dock( Pos::Top );
            row->SetKey( key );
            row->SetValue( pProp );
            
            // make sure the empty row is the last child
            if (m_emptyRow != NULL) {
                for (Base::List::iterator it = Children.begin(); it != Children.end(); ++it) {
                    if (*it == m_emptyRow) {
                        Children.erase(it);
                        break;
                    }
                }
                Children.push_back(m_emptyRow);
            }
            
            m_SplitterBar->BringToFront();
            return row;
        }
        
        void Properties::Clear()
        {
            Base::List ChildListCopy = Children;
            for ( Base::List::iterator it = ChildListCopy.begin(); it != ChildListCopy.end(); ++it )
            {
                PropertyRow* row = gwen_cast<PropertyRow>(*it);
                if ( !row ) continue;
                
                row->DelayedDelete();
            }
        }
        
        void Properties::SetSorted(bool sorted)
        {
            if (m_sorted == sorted)
                return;
            
            m_sorted = sorted;
            Invalidate();
        }
        
        void Properties::SetShowEmptyRow(bool showEmptyRow) 
        {
            if ((m_emptyRow != NULL) == showEmptyRow)
                return;
            
            if (m_emptyRow == NULL) {
                m_emptyRow = Add("", "");
                m_emptyRow->onKeyChange.Add(this, &Properties::EmptyPropertyChanged);
                m_emptyRow->onValueChange.Add(this, &Properties::EmptyPropertyChanged);
            } else {
                m_emptyRow->onKeyChange.RemoveHandler(this);
                m_emptyRow->onValueChange.RemoveHandler(this);
                m_emptyRow->DelayedDelete();
                m_emptyRow = NULL;
            }
            
            Invalidate();
        }

        void Properties::Think() {
            Base::Think();
            
            if (m_formerEmptyRow != NULL) {
                m_formerEmptyRow->onKeyChange.RemoveHandler(this);
                m_formerEmptyRow->onValueChange.RemoveHandler(this);
                m_formerEmptyRow = NULL;
            }
        }
        
        void Properties::EmptyPropertyChanged(Gwen::Controls::Base* control) {
            if (control != m_emptyRow)
                return;
            
            if (m_emptyRow->GetKey()->GetContent().empty()) {
                m_emptyRow->GetValue()->SetContent("");
                return;
            }
            
            m_formerEmptyRow = m_emptyRow;
            onRowAdd.Call(m_formerEmptyRow);

            m_emptyRow = Add("", "");
            m_emptyRow->onKeyChange.Add(this, &Properties::EmptyPropertyChanged);
            m_emptyRow->onValueChange.Add(this, &Properties::EmptyPropertyChanged);
        }
        
        GWEN_CONTROL_CONSTRUCTOR( PropertyRow )
        {
            m_Value = NULL;
            m_Key = new Property::Text(this);
            m_Key->Dock(Pos::Left);
            m_Key->onChange.Add(this, &PropertyRow::OnPropertyKeyChanged);
            m_Key->onHoverEnter.Add(this, &PropertyRow::OnChildHoverEnter);
            m_Key->onHoverLeave.Add(this, &PropertyRow::OnChildHoverLeave);
            m_deleteButton = NULL;
            m_removeDeleteButton = false;
            m_deletable = false;
        }	
        
        void PropertyRow::Render( Gwen::Skin::Base* skin )
        {
            /* SORRY */
            if ( IsKeyEditing() != m_bLastKeyEditing )
            {
                OnKeyEditingChanged();
                m_bLastKeyEditing = IsKeyEditing();
            }
            
            if ( IsValueEditing() != m_bLastValueEditing )
            {
                OnValueEditingChanged();
                m_bLastValueEditing = IsValueEditing();
            }
            /* SORRY */
            
            skin->DrawPropertyRow( this, m_Key->Right(), IsKeyEditing(), IsKeyHovered(), IsValueEditing(), IsValueHovered() );
        }
        
        void PropertyRow::Layout( Gwen::Skin::Base* /*skin*/ )
        {
            Properties* pParent = gwen_cast<Properties>( GetParent() );
            if ( !pParent ) return;
            
            m_Key->SetWidth( pParent->GetSplitWidth() );
            
            int height = m_Key->Height();
            if (m_Value)
                height = Gwen::Utility::Max(height, m_Value->Height());
            
            SetHeight(height);
        }
        
        void PropertyRow::SetKey( const TextObject& key )
        {
            m_Key->SetContent(key, true);
            m_OldKey = key;
        }
        
        void PropertyRow::SetValue( Property::Base* prop )
        {
            if (m_Value != NULL) {
                m_Value->onChange.RemoveHandler(this);
                m_Value->onHoverEnter.RemoveHandler(this);
                m_Value->onHoverLeave.RemoveHandler(this);
            }
            
            m_Value = prop;
            m_Value->SetParent( this );
            m_Value->Dock( Pos::Fill );
            m_Value->onChange.Add( this, &ThisClass::OnPropertyValueChanged );
            m_Value->onHoverEnter.Add(this, &PropertyRow::OnChildHoverEnter);
            m_Value->onHoverLeave.Add(this, &PropertyRow::OnChildHoverLeave);
        }
        
        void PropertyRow::SetDeletable(bool deletable) {
            if (deletable == m_deletable)
                return;
            
            m_deletable = deletable;
            if (!m_deletable && m_deleteButton != NULL && !m_removeDeleteButton)
                m_removeDeleteButton = true;
            else if (m_deletable && m_deleteButton == NULL) {
                m_deleteButton = createDeleteButton();
                m_removeDeleteButton = false;
            }
        }

        Button* PropertyRow::createDeleteButton()
        {
            Properties* pParent = gwen_cast<Properties>( GetParent() );
            
            Button* deleteButton = new Button(m_Key);
            deleteButton->SetBounds(pParent->GetSplitWidth() - Height() + 3, 2, Height() - 4, Height() - 4);
            deleteButton->SetText("X");
            deleteButton->onHoverEnter.Add(this, &PropertyRow::OnChildHoverEnter);
            deleteButton->onHoverLeave.Add(this, &PropertyRow::OnChildHoverLeave);
            deleteButton->onPress.Add(this, &PropertyRow::OnDeleteButtonPressed);
            return deleteButton;
        }

        
        void PropertyRow::OnPropertyKeyChanged( Gwen::Controls::Base* /*control*/ )
        {
            onKeyChange.Call( this );
            m_OldKey = m_Key->GetContent();
        }
        
        void PropertyRow::OnPropertyValueChanged( Gwen::Controls::Base* /*control*/ )
        {
            onValueChange.Call( this );
        }
        
        void PropertyRow::OnDeleteButtonPressed( Gwen::Controls::Base* control )
        {
            onDelete.Call(this);
        }

        void PropertyRow::OnKeyEditingChanged()
        {
            m_Value->Redraw();
        }
        
        void PropertyRow::OnValueEditingChanged()
        {
            m_Key->Redraw();
        }
        
        void PropertyRow::OnChildHoverEnter(Base* control)
        {
            if (m_deletable) {
                if (control == m_Key) {
                    if (m_deleteButton == NULL) {
                        m_deleteButton = createDeleteButton();
                    }
                    m_removeDeleteButton = false;
                } else if (control == m_deleteButton) {
                    m_removeDeleteButton = false;
                }
            }
            control->Redraw();
        }
        
        void PropertyRow::OnChildHoverLeave(Base* control)
        {
            if (m_deletable && (control == m_Key || control == m_deleteButton))
                m_removeDeleteButton = true;
            control->Redraw();
        }

        void PropertyRow::Think()
        {
            if (m_deleteButton != NULL && m_removeDeleteButton) {
                m_deleteButton->DelayedDelete();
                m_deleteButton->SetParent(NULL);
                m_deleteButton = NULL;
            }
            m_removeDeleteButton = false;
        }
    }
}