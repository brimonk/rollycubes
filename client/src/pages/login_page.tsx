import React from "react";
import { connect, DispatchProp } from "react-redux";
import { useLocation } from "react-router";
import { Link, RouteComponentProps } from "react-router-dom";
import { getCsrf } from "../auth";
import { selectAuthService } from "../selectors/game_selectors";
import { ReduxState } from "../store";
import "./login.css";

interface Props {
  route: RouteComponentProps;
  authService: string;
  authToken?: string | null;
}

function useQuery() {
  return new URLSearchParams(useLocation().search);
}
const LoginPage: React.FC<DispatchProp & Props> = (props) => {
  const query = useQuery();
  const [username, setUsername] = React.useState<string>(
    query.get("registered") || ""
  );
  const [password, setPassword] = React.useState("");

  const { route, authToken, authService, dispatch } = props;

  React.useEffect(() => {
    if (authToken) {
      route.history.replace("/home", {
        redirect: route.history.location.pathname,
      });
    }
  }, [route, authToken]);
  const login = async (e: any) => {
    e.preventDefault();
    const response = await window.fetch(authService + "login", {
      method: "post",
      mode: "cors",
      body: JSON.stringify({ username, password }),
      credentials: "include",
      headers: {
        "csrf-token": await getCsrf(authService),
        "Content-Type": "application/json",
      },
    });
    const { access_token } = await response.json();
    if (access_token) {
      dispatch({ type: "AUTHENTICATE", access_token });
    }
  };

  React.useEffect(() => {
    if (!document.cookie.includes("_session")) {
      route.history.replace("/", {
        redirect: route.history.location.pathname,
      });
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  if (!document.cookie.includes("_session")) {
    return null;
  }
  return (
    <div>
      <h1>Dice Game</h1>
      <p>the one where you roll some dice</p>
      <form onSubmit={login}>
        <div className="loginForm">
          <input
            onChange={(e) => setUsername(e.target.value)}
            placeholder="Username"
            value={username}
          />
          <input
            onChange={(e) => setPassword(e.target.value)}
            value={password}
            placeholder="Password"
            type="password"
          />
          <button type="submit" onClick={login}>
            Login
          </button>
        </div>
      </form>
      <p>
        Need an account? <Link to="/register">Register</Link>
      </p>
    </div>
  );
};

const mapStateToProps = (state: ReduxState) => {
  return {
    authService: selectAuthService(state),
    authToken: state.authToken,
  };
};

const ConnectedLoginPage = connect(mapStateToProps)(LoginPage);
export default (a: RouteComponentProps) => <ConnectedLoginPage route={a} />;
