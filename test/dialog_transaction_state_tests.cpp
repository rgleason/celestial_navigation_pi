#include <gtest/gtest.h>

#include "DialogTransactionState.h"

TEST(DialogTransactionState, IgnoresConstructorPopulationEvents) {
  DialogTransactionState state;
  state.MarkChanged();
  EXPECT_FALSE(state.HasUnsavedChanges());
}

TEST(DialogTransactionState, RecordsUserChangesAfterTrackingStarts) {
  DialogTransactionState state;
  state.StartTracking();
  EXPECT_FALSE(state.HasUnsavedChanges());
  state.MarkChanged();
  EXPECT_TRUE(state.HasUnsavedChanges());
}
