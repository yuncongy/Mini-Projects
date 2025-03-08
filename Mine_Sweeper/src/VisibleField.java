/**
  VisibleField class
  This is the data that's being displayed at any one point in the game (i.e., visible field, because
  it's what the user can see about the minefield). Client can call getStatus(row, col) for any 
  square.  It actually has data about the whole current state of the game, including the underlying
  mine field (getMineField()).  Other accessors related to game status: numMinesLeft(), 
  isGameOver().  It also has mutators related to actions the player could do (resetGameDisplay(),
  cycleGuess(), uncover()), and changes the game state accordingly.
  
  It, along with the MineField (accessible in mineField instance variable), forms the Model for the
  game application, whereas GameBoardPanel is the View and Controller in the MVC design pattern.  It
  contains the MineField that it's partially displaying.  That MineField can be accessed
  (or modified) from outside this class via the getMineField accessor.  
 */
public class VisibleField {
   // ----------------------------------------------------------   
   // The following public constants (plus values [0,8] mentioned in comments below) are the
   // possible states of one location (a "square") in the visible field (all are values that can be
   // returned by public method getStatus(row, col)).
   
   // The following are the covered states (all negative values):
   public static final int COVERED = -1;   // initial value of all squares
   public static final int MINE_GUESS = -2;
   public static final int QUESTION = -3;

   // The following are the uncovered states (all non-negative values):
   
   // values in the range [0,8] corresponds to number of mines adjacent to this opened square
   
   public static final int MINE = 9;      // this loc is a mine that hasn't been guessed already
                                          // (end of losing game)
   public static final int INCORRECT_GUESS = 10;  // is displayed a specific way at the end of
                                                  // losing game
   public static final int EXPLODED_MINE = 11;   // the one you uncovered by mistake (that caused
                                                 // you to lose)
   // ----------------------------------------------------------   

   private int[][] visibleField;
   private MineField mineField;
   private boolean gameOver;


   /**
      Create a visible field that has the given underlying mineField.
      The initial state will have all the locations covered, no mines guessed, and the game not
      over.
      @param mineField  the minefield to use for for this VisibleField
    */
   public VisibleField(MineField mineField) {
      this.mineField = mineField;
      gameOver = false;
      visibleField = new int[mineField.numRows()][mineField.numCols()];
      for (int i = 0; i < mineField.numRows(); i++) {
         for (int j = 0; j < mineField.numCols(); j++) {
            visibleField[i][j] = COVERED;
         }
      }
   }
   
   
   /**
      Reset the object to its initial state(Covered), using the same underlying MineField.
      Game is not over.
   */     
   public void resetGameDisplay() {
      gameOver = false;
      for (int i = 0; i < mineField.numRows(); i++) {
         for (int j = 0; j < mineField.numCols(); j++) {
            visibleField[i][j] = COVERED;
         }
      }
   }
  
   
   /**
      Returns a reference to the mineField that this VisibleField "covers"
      @return the minefield
    */
   public MineField getMineField() {
      return mineField;
   }
   
   
   /**
      Returns the visible status of the square indicated.
      @param row  row of the square
      @param col  col of the square
      @return the status of the square at location (row, col).  See the public constants at the
            beginning of the class for the possible values that may be returned, and their meanings.
      PRE: getMineField().inRange(row, col)
    */
   public int getStatus(int row, int col) {
      return visibleField[row][col];
   }

   
   /**
      Returns the the number of mines left to guess.  This has nothing to do with whether the mines
      guessed are correct or not.  Just gives the user an indication of how many more mines the user
      might want to guess.  This value will be negative if they have guessed more than the number of
      mines in the minefield.
      @return the number of mines left to guess.
    */
   public int numMinesLeft() {
      int guessedMines = 0;
      for (int i = 0; i < mineField.numRows(); i++) {
         for (int j = 0; j < mineField.numCols(); j++) {
            if (visibleField[i][j] == MINE_GUESS) {
               guessedMines++;
            }
         }
      }
      return mineField.numMines() - guessedMines;
   }
 
   
   /**
      Cycles through covered states for a square, updating number of guesses as necessary.  Call on
      a COVERED square changes its status to MINE_GUESS; call on a MINE_GUESS square changes it to
      QUESTION;  call on a QUESTION square changes it to COVERED again; call on an uncovered square
      has no effect.  
      @param row  row of the square
      @param col  col of the square
      PRE: getMineField().inRange(row, col)
    */
   public void cycleGuess(int row, int col) {
         if (visibleField[row][col] == COVERED) {
            visibleField[row][col] = MINE_GUESS;  // Change COVERED to MINE_GUESS
         }
         else if (visibleField[row][col] == MINE_GUESS) {
            visibleField[row][col] = QUESTION;    // Change MINE_GUESS to QUESTION
         }
         else if (visibleField[row][col] == QUESTION) {
            visibleField[row][col] = COVERED;     // Change QUESTION back to COVERED
         }
         else {
            return;
         }
   }

   
   /**
      Uncovers this square and returns false iff you uncover a mine here.
      If the square wasn't a mine or adjacent to a mine it also uncovers all the squares in the
      neighboring area that are also not next to any mines, possibly uncovering a large region.
      Any mine-adjacent squares you reach will also be uncovered, and form (possibly along with
      parts of the edge of the whole field) the boundary of this region.
      Does not uncover, or keep searching through, squares that have the status MINE_GUESS. 
      Note: this action may cause the game to end: either in a win (opened all the non-mine squares)
      or a loss (opened a mine).
      @param row  of the square
      @param col  of the square
      @return false   iff you uncover a mine at (row, col)
      PRE: getMineField().inRange(row, col)
    */
   public boolean uncover(int row, int col) {
      if (getMineField().hasMine(row, col)) {
            visibleField[row][col] = EXPLODED_MINE; // Mark this square as the one that caused the game over
            gameOver = true;
            lose_game_display();
            return false;
      }
      else {
            uncoverNeighbors(row, col);
            return true;
      }
   }
 
   
   /**
      Returns whether the game is over.
      Checks the number of squares uncovered and compare it with target value to check if all possible
      non-mine squares have been discovered. If yes, change to win_game_display.
      @return whether game has ended
    */
   public boolean isGameOver() {
      if (gameOver) {
         return true;
      }
      int num_uncovered = 0;
      for (int i = 0; i < visibleField.length; i++) {
         for (int j = 0; j < visibleField[i].length; j++) {
            if (isUncovered(i, j)) {
               num_uncovered++;
            }
         }
      }
      if (num_uncovered == mineField.numCols() * mineField.numRows() - mineField.numMines()) {
         gameOver = true;
         win_game_display();

      }
      return gameOver;
   }
 
   
   /**
      Returns whether this square has been uncovered.  (i.e., is in any one of the uncovered states, 
      vs. any one of the covered states).
      @param row of the square
      @param col of the square
      @return whether the square is uncovered
      PRE: getMineField().inRange(row, col)
    */
   public boolean isUncovered(int row, int col) {
      return visibleField[row][col] >= 0;
   }


   /**
    * Recursively uncovers neighboring squares around (row, col) if there are no adjacent mines.
    * Stops at cells with mines adjacent or at MINE_GUESS.
    * @param row  the row of the square
    * @param col  the column of the square
    * PRE: getMineField().inRange(row, col)
    */
   private void uncoverNeighbors(int row, int col) {
      if (!getMineField().inRange(row, col) || isUncovered(row, col) || visibleField[row][col] == MINE_GUESS) {
         return;
      }

      // do recursive search in 8 directions: up down left right and 4 diagonals
      if (mineField.numAdjacentMines(row, col) == 0) {
         visibleField[row][col] = 0;
         uncoverNeighbors(row - 1, col);
         uncoverNeighbors(row, col - 1);
         uncoverNeighbors(row + 1, col);
         uncoverNeighbors(row, col + 1);
         uncoverNeighbors(row - 1, col - 1);
         uncoverNeighbors(row + 1, col + 1);
         uncoverNeighbors(row + 1, col - 1);
         uncoverNeighbors(row - 1, col + 1);
      }
      else{
         visibleField[row][col] = mineField.numAdjacentMines(row, col);
         return;
      }
   }


   /**
    *   Updates the display to show a winning state by marking all remaining covered squares as MINE_GUESS.
    *   This indicates that the player has correctly uncovered all non-mine squares
    */
   private void win_game_display(){
      for (int i = 0; i < mineField.numRows(); i++) {
         for (int j = 0; j < mineField.numCols(); j++) {
            if (visibleField[i][j] == COVERED) {
               visibleField[i][j] = MINE_GUESS;
            }
         }
      }
   }


   /**
    * Updates the display when the game is lost
    * The exploded mine is shown by a red square.
    * Any previously made incorrect guesses are shown with an X though them,
    * the correctly guessed mine locations are still shown as guesses (yellow),
    * and the other unopened mines are shown as "mines"
    */
   private void lose_game_display(){
      for (int i = 0; i < mineField.numRows(); i++) {
         for (int j = 0; j < mineField.numCols(); j++) {
            if (mineField.hasMine(i, j) && visibleField[i][j] != EXPLODED_MINE && visibleField[i][j] != MINE_GUESS) {
               visibleField[i][j] = MINE;
            }
            else{
               if (visibleField[i][j] == MINE_GUESS && !mineField.hasMine(i, j)) {
                  visibleField[i][j] = INCORRECT_GUESS;
               }
            }
         }
      }
   }
}
