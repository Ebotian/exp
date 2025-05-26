// filepath: hc25_fullstack_control/frontend/src/App.jsx
import React, { useState, useEffect, useCallback } from 'react';
import axios from 'axios';
import './App.css'; // 你可以创建一个 App.css 来美化界面

// 后端 API 的基础 URL
// 在开发中，Vite 开发服务器通常运行在不同端口 (例如 5173)
// Flask 服务器运行在 5000
const API_BASE_URL = '/api'; // 修改为你的后端地址

function App() {
  const [history, setHistory] = useState([]);
  const [statusMessage, setStatusMessage] = useState('');
  const [moduleStatus, setModuleStatus] = useState({ connected: false, address: null });
  const [isLoading, setIsLoading] = useState(false);

  const fetchHistory = useCallback(async () => {
    try {
      const response = await axios.get(`${API_BASE_URL}/history`);
      setHistory(response.data);
    } catch (error) {
      console.error("获取历史记录失败:", error);
      setStatusMessage('获取历史记录失败: ' + (error.response?.data?.message || error.message));
    }
  }, []);

  const fetchModuleStatus = useCallback(async () => {
    try {
      const response = await axios.get(`${API_BASE_URL}/status`);
      setModuleStatus({
        connected: response.data.module_connected,
        address: response.data.module_address
      });
    } catch (error) {
      console.error("获取模块状态失败:", error);
       setModuleStatus({ connected: false, address: "获取状态失败" });
    }
  }, []);


  useEffect(() => {
    fetchHistory();
    fetchModuleStatus();
    // 定时刷新状态和历史
    const intervalId = setInterval(() => {
      fetchHistory();
      fetchModuleStatus();
    }, 5000); // 每5秒刷新一次

    return () => clearInterval(intervalId); // 组件卸载时清除定时器
  }, [fetchHistory, fetchModuleStatus]);

  const sendCharacter = async (char) => {
    setIsLoading(true);
    setStatusMessage(`正在发送字符 '${char}'...`);
    try {
      const response = await axios.post(`${API_BASE_URL}/send_command`, { char });
      setStatusMessage(response.data.message);
      if (response.data.success) {
        // 成功后立即刷新历史，而不是等待定时器
        fetchHistory();
      }
    } catch (error) {
      console.error(`发送字符 '${char}' 失败:`, error);
      setStatusMessage(`发送字符 '${char}' 失败: ` + (error.response?.data?.message || error.message));
    } finally {
      setIsLoading(false);
    }
  };

  const charsToSend = ['A', 'B', 'C', 'D'];

  return (
    <div className="container">
      <h1>物联网模块远程控制器</h1>

      <div className="status-panel">
        <h2>模块状态</h2>
        <p>
          连接状态: <span className={moduleStatus.connected ? 'connected' : 'disconnected'}>
            {moduleStatus.connected ? '已连接' : '未连接'}
          </span>
        </p>
        {moduleStatus.connected && <p>模块地址: {moduleStatus.address}</p>}
      </div>

      <div className="control-panel">
        <h2>发送指令</h2>
        {charsToSend.map(char => (
          <button
            key={char}
            onClick={() => sendCharacter(char)}
            disabled={isLoading || !moduleStatus.connected}
            className="control-button"
          >
            发送 '{char}'
          </button>
        ))}
      </div>

      {statusMessage && <div className={`status-message ${statusMessage.includes('错误') || statusMessage.includes('失败') ? 'error' : 'success'}`}>{statusMessage}</div>}

      <div className="history-panel">
        <h2>发送历史</h2>
        {history.length === 0 && <p>暂无历史记录。</p>}
        <ul>
          {history.map((item, index) => (
            <li key={index} className={`history-item ${item.status}`}>
              <span className="timestamp">{item.timestamp}</span>
              {item.event === "Send Command" && <span className="char-sent">发送: '{item.char_sent}'</span>}
              <span className="message">{item.event}: {item.message}</span>
            </li>
          ))}
        </ul>
      </div>
    </div>
  );
}

export default App;