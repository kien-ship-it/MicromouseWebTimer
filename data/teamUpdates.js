// teamUpdates.js
// Global variable to store current team ID
let currentTeamId = null;

function selectTeam(teamId) {
    console.log('Selecting team:', teamId);
    
    // Disable the team selection temporarily to prevent double-clicks
    const teamElements = document.querySelectorAll('[onclick^="selectTeam"]');
    teamElements.forEach(elem => elem.style.pointerEvents = 'none');
    
    fetch('/select-team', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: 'id=' + encodeURIComponent(teamId)
    })
    .then(response => {
        console.log('Response received:', response);
        if (!response.ok) {
            throw new Error('Network response was not ok');
        }
        return response.json();
    })
    .then(data => {
        console.log('Data received:', data);
        if (data.status === 'success') {
            // Store the selected team ID in localStorage
            localStorage.setItem('currentTeamId', teamId);
            // Redirect to team.html
            window.location.href = '/team.html';
        } else {
            console.error('Team selection failed:', data.message);
            alert('Failed to select team. Please try again.');
            // Re-enable team selection
            teamElements.forEach(elem => elem.style.pointerEvents = 'auto');
        }
    })
    .catch(error => {
        console.error('Error:', error);
        alert('An error occurred. Please try again.');
        // Re-enable team selection
        teamElements.forEach(elem => elem.style.pointerEvents = 'auto');
    });
}

function updateTeamPage(teamData) {
    // Update team name
    document.querySelector('.team-header h1').textContent = `Đội ${teamData.id}`;
    
    // Find current active attempt
    const currentAttempt = teamData.attempts.find(a => !a.completed);
    const currentTimeElement = document.querySelector('.timer-box .time');
    
    if (currentAttempt && currentAttempt.timeTaken !== null) {
        currentTimeElement.textContent = currentAttempt.timeTaken.toFixed(3) + 's';
    } else {
        currentTimeElement.textContent = '0.000s';
    }
    
    // Update completed attempts
    const attemptsList = document.querySelector('.attempts-list');
    
    // Find the current attempt index (first uncompleted attempt)
    const currentAttemptIndex = teamData.attempts.findIndex(a => !a.completed);
    
    attemptsList.innerHTML = teamData.attempts.map((attempt, index) => `
        <div class="attempt ${index === currentAttemptIndex ? 'current' : ''}">
            <span>Lượt ${index + 1}</span>
            <span>${attempt.completed ? attempt.timeTaken.toFixed(3) + 's' : 'Chưa thực hiện'}</span>
        </div>
    `).join('');
}

function updateIndexPage(data) {
    // Validate the data structure
    if (!data || typeof data !== 'object' || !Array.isArray(data.teams)) {
        console.error('Invalid or missing data:', data);
        return;
    }

    // Process the teams data using data.teams instead of teamsData
    data.teams.forEach(team => {
        const teamElement = document.querySelector(`[onclick="selectTeam(${team.id})"]`);
        if (teamElement) {
            const teamTimeElement = teamElement.querySelector('.team-time');
            const bestTime = team.bestTime > 0 ? team.bestTime.toFixed(3) + 's' : 'Chưa có';
            teamTimeElement.textContent = bestTime;
        }
    });
}


// Function to fetch and update data
function fetchTeamData() {
    if (window.location.pathname.includes('team.html')) {
        // On team page, fetch specific team data
        fetch('team-data')
            .then(response => response.json())
            .then(data => updateTeamPage(data))
            .catch(error => console.error('Error:', error));
    } else {
        // On index page, fetch all teams data
        fetch('teams-data')
            .then(response => response.json())
            .then(data => updateIndexPage(data))
            .catch(error => console.error('Error:', error));
    }
}

// Start polling when page loads
document.addEventListener('DOMContentLoaded', () => {
    // Initialize team page if needed
    initTeamPage();
    
    // Start periodic updates
    setInterval(fetchTeamData, 100);

    // Reset button functionality (if exists)
    const resetBtn = document.querySelector('.reset-btn');
    if (resetBtn) {
        resetBtn.addEventListener('click', () => {
            const teamId = localStorage.getItem('currentTeamId');
            if (!teamId) return;

            fetch('/reset-team', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: 'id=' + encodeURIComponent(teamId)
            })
            .then(response => response.json())
            .then(data => {
                if (data.status === 'success') {
                    // Refresh the page data
                    fetchTeamData();
                } else {
                    console.error('Reset failed:', data.message);
                    alert('Failed to reset team data. Please try again.');
                }
            })
            .catch(error => {
                console.error('Error:', error);
                alert('An error occurred while resetting. Please try again.');
            });
        });
    }
});

function initTeamPage() {
    const teamId = localStorage.getItem('currentTeamId');
    if (!teamId && window.location.pathname.includes('team.html')) {
        // If no team is selected and we're on team.html, redirect to index
        window.location.href = '/index.html';
        return;
    }

    // If we're on team.html, immediately fetch team data
    if (window.location.pathname.includes('team.html')) {
        fetch('/select-team', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: 'id=' + encodeURIComponent(teamId)
        })
        .then(response => response.json())
        .then(data => {
            if (data.status !== 'success') {
                console.error('Failed to initialize team data');
                window.location.href = '/index.html';
            }
        })
        .catch(error => {
            console.error('Error initializing team data:', error);
            window.location.href = '/index.html';
        });
    }
}