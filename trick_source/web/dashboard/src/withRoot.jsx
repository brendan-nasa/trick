import React from 'react';
import { ThemeProvider, StyledEngineProvider, createTheme } from '@mui/material/styles';
//import cyan from '@mui/material/colors/cyan';
//import deepPurple from '@mui/material/colors/deepPurple';
//import green from '@mui/material/colors/green';
//import blueGrey from '@mui/material/colors/blueGrey';
import CssBaseline from '@mui/material/CssBaseline';

// A theme with custom primary and secondary color.
// It's optional.
const theme = createTheme({
  palette: {
    primary: {main: "#5d6a73" /* #333f48 is dark */},
    secondary: {main: "#bf5700" /* choose f88638 for a lighter color */},
    type:"dark",
  },
});

function withRoot(Component) {
  function WithRoot(props) {
    // MuiThemeProvider makes the theme available down the React tree
    // thanks to React context.
    return (
      <StyledEngineProvider injectFirst>
        (<MuiThemeProvider theme={theme}>
          {/* CssBaseline kickstart an elegant, consistent, and simple baseline to build upon. */}
          <CssBaseline />
          <Component {...props} />
        </MuiThemeProvider>)
      </StyledEngineProvider>
    );
  }

  return WithRoot;
}

export default withRoot;
