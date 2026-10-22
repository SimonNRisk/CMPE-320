import { useState } from "react";
import "./App.css";

function App() {
  //to change between faculties set to null to be at home page
  const [selectedFaculty, setSelectedFaculty] = useState(null);
// fac options
  const faculties = [
    "Engineering",
    "Health Science",
    "Arts & Science",
    "Commerce",
    "Concurrent Education"
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
        <main className="faculty-page">
          <button
            className="back-button"
            onClick={() => setSelectedFaculty(null)}
          >
           Back to Home
          </button>

          <h2>{selectedFaculty}</h2>
          <p>Welcome to the {selectedFaculty} section!</p>
        </main>
      )}

      <footer>
      </footer>
    </div>
  );
}

export default App;


