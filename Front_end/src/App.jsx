import { useState } from "react";
import "./App.css";

function App() {
  //to change between faculties set to null to be at home page
  const [selectedFaculty, setSelectedFaculty] = useState(null);
  const [selectedLevel, setSelectedLevel] = useState(null);
// fac options
  const faculties = [
    "Engineering",
    "Health Science",
    "Arts & Science",
    "Commerce",
    "Concurrent Education"
  ];
  const levels = [
    "100",
    "200",
    "300",
    "400",
    "500"
  ];

  return (
    //header
    <div className="website">
      <header className="header">
        {/* change back to null once at home page */}

        <h1 onClick={() => setSelectedFaculty(null)}>
          Queen's University
        </h1>
        <p>Extra Resources</p>
      </header>

      <div className="search-bar">
        <input type="text" placeholder="Search for a specific course..." />
      </div>

     {/* selection */}
      {selectedFaculty === null ? (
        <main className="home">
          <p>Select your faculty to get started</p>

          <div className="faculty-grid">
            {faculties.map((faculty) => (
              <button
                key={faculty}
                className="faculty-card"
                //go to faculty subsection
                onClick={() => setSelectedFaculty(faculty)}
              >
                <h3>{faculty}</h3>
                <span>Explore</span>
              </button>
            ))}
          </div>
        </main>
      ) : (
        //subsections
        selectedLevel === null ? (
          <main className="faculty-page">
            <button
              className="back-button"
              onClick={() => setSelectedFaculty(null)}
            >
            Back to Home
            </button>

            <h2>{selectedFaculty}</h2>
            <p>Welcome to the {selectedFaculty} section! Select course level to find your courses!</p>

            <div className="level-grid">
              {levels.map((level) => (
                <button
                  key={level}
                  className="level-card"
                  //go to faculty subsection
                  onClick={() => setSelectedLevel(level)}
                >
                  <h3>{level}</h3>
                </button>
              ))}
            </div>
          </main>
      ) : (
        <main className="level-page">
          <button
            className="faculty-back-button"
            onClick={() => setSelectedLevel(null)}
          >
           Back to Level Selection
          </button>

          <h2 style = {{marginBottom: '0px'}} onClick={() => setSelectedLevel(null)}>{selectedFaculty}</h2> <h4 style = {{marginTop: '0px'}}> {selectedLevel} </h4>
          <p>Welcome to the {selectedFaculty} {selectedLevel} level courses!</p>
        </main>
      ))}

      <footer>
      </footer>
    </div>
  );
}

export default App;


