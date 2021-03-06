#include "ROOT/RDFDisplay.hxx"
#include "TInterpreter.h"

#include <iomanip>

namespace ROOT {
namespace Internal {
namespace RDF {
RDisplayElement::RDisplayElement(const std::string &representation) : fRepresentation(representation)
{
   SetPrint();
}

RDisplayElement::RDisplayElement()
{
   SetPrint();
}

void RDisplayElement::SetPrint()
{
   fPrintingAction = PrintingAction::ToBePrinted;
}

void RDisplayElement::SetIgnore()
{
   fPrintingAction = PrintingAction::ToBeIgnored;
}

void RDisplayElement::SetDots()
{
   fPrintingAction = PrintingAction::ToBeDotted;
}

bool RDisplayElement::IsPrint() const
{
   return fPrintingAction == PrintingAction::ToBePrinted;
}

bool RDisplayElement::IsIgnore() const
{
   return fPrintingAction == PrintingAction::ToBeIgnored;
}

bool RDisplayElement::IsDot() const
{
   return fPrintingAction == PrintingAction::ToBeDotted;
}

std::string RDisplayElement::GetRepresentation() const
{
   return fRepresentation;
}

bool RDisplayElement::IsEmpty() const
{
   return fRepresentation.empty();
}

} // namespace RDF
} // namespace Internal

namespace RDF {
void RDisplay::AddToRow(const std::string &stringEle)
{
   // If the current element is wider than the widest element found, update the width
   if (fWidths[fCurrentColumn] < stringEle.length()) {
      fWidths[fCurrentColumn] = stringEle.length();
   }
   // Save the element...
   fTable[fCurrentRow][fCurrentColumn] = DElement_t(stringEle);

   // ...and move to the next
   MovePosition();
}

void RDisplay::AddCollectionToRow(const std::vector<std::string> &collection)
{
   auto row = fCurrentRow;
   // For each element of the collection, save it. The first element will be in the current row, next ones will have
   // their own row.
   size_t collectionSize = collection.size();
   for (size_t index = 0; index < collectionSize; ++index) {
      auto stringEle = collection[index];
      auto element = DElement_t(stringEle);

      // Update the width if this element is the biggest found
      if (fWidths[fCurrentColumn] < stringEle.length()) {
         fWidths[fCurrentColumn] = stringEle.length();
      }

      if (index == 0 || index == collectionSize - 1) {
         // Do nothing, by default DisplayElement is printed
      } else if (index == 1) {
         element.SetDots();
         // Be sure the "..." fit
         if (fWidths[fCurrentColumn] < 3) {
            fWidths[fCurrentColumn] = 3;
         }
      } else {
         // In the Print(), after the dots, all element will just be ignored except the last one.
         element.SetIgnore();
      }

      // Save the element
      fTable[row][fCurrentColumn] = element;
      ++row;

      if (index != collectionSize - 1 && fTable.size() <= row) {
         // This is not the last element, prepare the next row for the next element, if not already done by another
         // collection
         fTable.push_back(std::vector<DElement_t>(fNColumns));
      }
   }
   fNextRow = (fNextRow > row) ? fNextRow : row;
   MovePosition();
}

void RDisplay::MovePosition()
{
   // Go to the next element. If it is outside the row, just go the first element of the next row.
   ++fCurrentColumn;
   if (fCurrentColumn == fNColumns) {
      fCurrentRow = fNextRow;
      fCurrentColumn = 0;
      fNextRow = fCurrentRow + 1;
      fTable.push_back(std::vector<DElement_t>(fNColumns));
   }
}

void RDisplay::CallInterpreter(const std::string &code)
{
   TInterpreter::EErrorCode errorCode;
   gInterpreter->Calc(code.c_str(), &errorCode);
   if (TInterpreter::EErrorCode::kNoError != errorCode) {
      std::string msg = "Cannot jit during Display call. Interpreter error code is " + std::to_string(errorCode) + ".";
      throw std::runtime_error(msg);
   }
}

RDisplay::RDisplay(const VecStr_t &columnNames, const VecStr_t &types, const int &entries)
   : fTypes(types), fWidths(columnNames.size(), 0), fRepresentations(columnNames.size()),
     fCollectionsRepresentations(columnNames.size()), fNColumns(columnNames.size()), fEntries(entries)
{

   // Add the first row with the names of the columns
   fTable.push_back(std::vector<DElement_t>(columnNames.size()));
   for (auto name : columnNames) {
      AddToRow(name);
   }
}

size_t RDisplay::GetNColumnsToShorten() const
{
   size_t totalWidth = 0;

   auto size = fWidths.size();
   for (size_t i = 0; i < size; ++i) {
      totalWidth += fWidths[i];
      if (totalWidth > fgMaxWidth) {
         return size - i;
      }
   }

   return 0;
}

void RDisplay::Print() const
{
   auto columnsToPrint =
      fNColumns - GetNColumnsToShorten(); // Get the number of columns that fit in the characters limit
   std::vector<bool> hasPrintedNext(fNColumns,
                                    false); // Keeps track if the collection as already been shortened, allowing to skip
                                            // all elements until the next printable element.
   auto nrRows = fTable.size();
   for (size_t rowIndex = 0; rowIndex < nrRows; ++rowIndex) {
      auto &row = fTable[rowIndex];

      std::stringstream stringRow;
      bool isRowEmpty = true; // It may happen during compacting that some rows are empty, this happens for example if
                              // collections have different size. Thanks to this flag, these rows are just ignored.
      for (size_t columnIndex = 0; columnIndex < columnsToPrint; ++columnIndex) {
         const auto &element = row[columnIndex];
         std::string printedElement = "";

         if (element.IsDot()) {
            printedElement = "...";
         } else if (element.IsPrint()) {
            // Maybe the element is part of a collection that is being shortened, and so it was already printed.
            if (!hasPrintedNext[columnIndex]) {
               printedElement = element.GetRepresentation();
            }
            hasPrintedNext[columnIndex] =
               false; // Encountered "next printable element", shortening can start again when needed.
         } else {     // IsIgnore
            // Shortening is starting here. Print directly the last element, to have something like 1 ... 3, and don't
            // print anything else.
            if (!hasPrintedNext[columnIndex]) {
               size_t i = rowIndex + 1; // Starting from the next row...
               for (; !fTable[i][columnIndex].IsPrint(); ++i) {
                  // .. look for the first element that can be printed, it will be the last of the collection.
               }
               printedElement = fTable[i][columnIndex].GetRepresentation(); // Print the element
               hasPrintedNext[columnIndex] = true; // And start ignoring anything else until the next collection.
            }
         }
         if (!printedElement.empty()) {
            // Found at least one element, so the row is not empty.
            isRowEmpty = false;
         }

         stringRow << std::left << std::setw(fWidths[columnIndex]) << std::setfill(fgSeparator) << printedElement
                   << " | ";
      }
      if (!isRowEmpty) {
         std::cout << stringRow.str() << std::endl;
      }
   }
}

std::string RDisplay::AsString() const
{
   // This method works as Print() but without any check on collection. It just returns a string with the whole
   // representation
   std::stringstream stringRepresentation;
   for (auto row : fTable) {
      for (size_t i = 0; i < row.size(); ++i) {
         stringRepresentation << std::left << std::setw(fWidths[i]) << std::setfill(fgSeparator)
                              << row[i].GetRepresentation() << " | ";
      }
      stringRepresentation << "\n";
   }
   return stringRepresentation.str();
}

} // namespace RDF
} // namespace ROOT
