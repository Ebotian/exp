#!/bin/bash

# --- 配置变量 ---
REMOTE_USER="admin"
REMOTE_HOST="39.107.106.220"
SSH_KEY="~/.ssh/id_rsa"
SSH_CMD="ssh -i ${SSH_KEY} ${REMOTE_USER}@${REMOTE_HOST}"
SCP_CMD="scp -i ${SSH_KEY}"

LOCAL_FRONTEND_DIR="./"
LOCAL_BACKEND_DIR="./backend"

REMOTE_BASE_DIR="/var/www/smart_light"
REMOTE_FRONTEND_DIR="${REMOTE_BASE_DIR}/frontend_dist"
REMOTE_BACKEND_DIR="${REMOTE_BASE_DIR}/backend"
REMOTE_VENV_DIR="${REMOTE_BACKEND_DIR}/venv"

NGINX_CONF_NAME="smart_light_web"
NGINX_SITES_AVAILABLE="/etc/nginx/sites-available"
NGINX_SITES_ENABLED="/etc/nginx/sites-enabled"

BACKEND_PORT="5002"
FRONTEND_PORT="120"

# --- 函数定义 ---
echo_color() {
  COLOR=$1
  TEXT=$2
  case $COLOR in
  "green") echo -e "\033[32m${TEXT}\033[0m" ;;
  "red") echo -e "\033[31m${TEXT}\033[0m" ;;
  "yellow") echo -e "\033[33m${TEXT}\033[0m" ;;
  *) echo "${TEXT}" ;;
  esac
}

# --- 脚本开始 ---
echo_color "yellow" "开始部署 smart-light-web 应用..."

# 1. 本地构建前端
cd "${LOCAL_FRONTEND_DIR}" || {
  echo_color "red" "错误: 无法进入前端目录 ${LOCAL_FRONTEND_DIR}"
  exit 1
}
echo_color "yellow" "1. 正在本地构建前端应用..."
npm install || {
  echo_color "red" "错误: npm install 失败"
  exit 1
}
npm run build || {
  echo_color "red" "错误: npm run build 失败"
  exit 1
}
echo_color "green" "前端构建完成。"
# cd ..  # 不要切换目录，否则找不到 dist

# 2. 在服务器上创建目录结构
echo_color "yellow" "2. 正在服务器上创建目录结构..."
${SSH_CMD} "sudo mkdir -p ${REMOTE_FRONTEND_DIR} && \
            sudo mkdir -p ${REMOTE_BACKEND_DIR} && \
            sudo chown -R ${REMOTE_USER}:${REMOTE_USER} ${REMOTE_BASE_DIR}" ||
  {
    echo_color "red" "错误: 无法在服务器上创建目录"
    exit 1
  }
echo_color "green" "服务器目录结构准备就绪。"

# 3. 上传文件到服务器
echo_color "yellow" "3. 正在上传文件到服务器..."
${SCP_CMD} -r "${LOCAL_FRONTEND_DIR}/dist/." "${REMOTE_USER}@${REMOTE_HOST}:${REMOTE_FRONTEND_DIR}/" ||
  {
    echo_color "red" "错误: 上传前端文件失败"
    exit 1
  }
${SCP_CMD} -r "${LOCAL_BACKEND_DIR}/server.py" "${REMOTE_USER}@${REMOTE_HOST}:${REMOTE_BACKEND_DIR}/" ||
  {
    echo_color "red" "错误: 上传后端 server.py 失败"
    exit 1
  }
# 如有 requirements.txt 也上传
if [ -f "${LOCAL_BACKEND_DIR}/requirements.txt" ]; then
  ${SCP_CMD} "${LOCAL_BACKEND_DIR}/requirements.txt" "${REMOTE_USER}@${REMOTE_HOST}:${REMOTE_BACKEND_DIR}/"
fi
echo_color "green" "文件上传完成。"

# 4. 在服务器上设置后端环境并启动后端
echo_color "yellow" "4. 正在服务器上设置并启动后端应用..."
${SSH_CMD} <<EOF_BACKEND
    set -e # Exit immediately if a command exits with a non-zero status.
    echo "进入后端目录 ${REMOTE_BACKEND_DIR}"
    cd "${REMOTE_BACKEND_DIR}" || { echo "错误: 无法进入后端目录 ${REMOTE_BACKEND_DIR}"; exit 1; }

    if [ ! -d "${REMOTE_VENV_DIR}" ]; then
        echo "创建虚拟环境 ${REMOTE_VENV_DIR}..."
        python3 -m venv venv || { echo "错误: 创建虚拟环境失败"; exit 1; }
        echo "虚拟环境已创建。"
    fi

    echo "激活虚拟环境..."
    source "${REMOTE_VENV_DIR}/bin/activate" || { echo "错误: 激活虚拟环境失败"; exit 1; }

    echo "安装依赖..."
    if [ -f "requirements.txt" ]; then
        pip install -r requirements.txt || { echo "错误: pip install -r requirements.txt 失败"; exit 1; }
    else
        pip install flask flask-cors || { echo "错误: pip install flask flask-cors 失败"; exit 1; }
    fi

    echo "停止可能正在运行的旧后端进程..."
    # 尝试更强制地停止使用端口的进程
    sudo fuser -k 5002/tcp || echo "端口 5002 未被占用或无法终止进程。"
    sudo fuser -k 9000/tcp || echo "端口 9000 未被占用或无法终止进程。"
    # 再次尝试 pkill 以防万一
    pkill -f "python server.py" || echo "没有找到名为 'python server.py' 的进程。"
    sleep 2 # 等待进程完全终止
    echo "旧的 server.py 进程已停止。"

    echo "在后台启动新的 server.py 进程..."
    nohup python server.py > backend.log 2>&1 &
    SERVER_PID=\\$!
    echo "server.py 尝试以 PID \${SERVER_PID} 启动。日志将输出到 backend.log。"
    sleep 5 # 给服务器一点时间启动和绑定端口

    echo "检查后端 TCP 服务 (端口 9000) 是否在监听..."
    if sudo netstat -tulnp | grep ':9000.*LISTEN' | grep -q "python"; then
        echo "后端 TCP 服务 (9000) 正在监听。"
    else
        echo "错误: 后端 TCP 服务 (9000) 未在监听。请检查 backend.log。"
        tail -n 30 backend.log # 显示日志尾部帮助诊断
        exit 1
    fi

    echo "检查后端 HTTP 服务 (端口 ${BACKEND_PORT}) 是否在监听..."
    if sudo netstat -tulnp | grep ":${BACKEND_PORT}.*LISTEN" | grep -q "python"; then
        echo "后端 HTTP 服务 (${BACKEND_PORT}) 正在监听。"
    else
        echo "错误: 后端 HTTP 服务 (${BACKEND_PORT}) 未在监听。请检查 backend.log。"
        tail -n 30 backend.log # 显示日志尾部帮助诊断
        exit 1
    fi

    deactivate
    echo "后端应用已成功启动并验证。"
EOF_BACKEND
if [ $? -ne 0 ]; then # This will now catch failures from within the heredoc if set -e was not enough or if the heredoc itself failed.
  echo_color "red" "错误: 服务器后端设置或启动失败 (SSH命令块返回错误)"
  exit 1
fi
echo_color "green" "服务器后端设置并尝试启动。"

# 5. 在服务器上配置 Nginx
echo_color "yellow" "5. 正在服务器上配置 Nginx..."
NGINX_CONFIG_CONTENT="server {
    listen ${FRONTEND_PORT};
    server_name ${REMOTE_HOST};
    root ${REMOTE_FRONTEND_DIR};
    index index.html index.htm;
    location / {
        try_files \$uri \$uri/ /index.html;
    }
    location /api/ {
        proxy_pass http://localhost:${BACKEND_PORT}/api/;
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;
    }
    access_log /var/log/nginx/${NGINX_CONF_NAME}.access.log;
    error_log /var/log/nginx/${NGINX_CONF_NAME}.error.log;
}"
${SSH_CMD} "echo '${NGINX_CONFIG_CONTENT}' | sudo tee ${NGINX_SITES_AVAILABLE}/${NGINX_CONF_NAME} > /dev/null" ||
  {
    echo_color "red" "错误: 无法在服务器上写入 Nginx 配置文件"
    exit 1
  }
${SSH_CMD} "sudo ln -sf ${NGINX_SITES_AVAILABLE}/${NGINX_CONF_NAME} ${NGINX_SITES_ENABLED}/${NGINX_CONF_NAME}" ||
  {
    echo_color "red" "错误: 无法创建 Nginx 站点软链接"
    exit 1
  }
${SSH_CMD} "sudo nginx -t && sudo systemctl reload nginx"
if [ $? -ne 0 ]; then
  echo_color "red" "错误: Nginx 配置测试失败或无法重新加载 Nginx。"
  echo_color "yellow" "请登录服务器检查 Nginx 错误: sudo nginx -t 和 journalctl -xeu nginx.service"
  exit 1
fi
echo_color "green" "Nginx 配置完成并已重新加载。"

echo_color "green" "部署完成！"
echo_color "yellow" "请检查以下事项："
echo_color "yellow" "- 确保你的 ESP32 已配置为连接到 ${REMOTE_HOST} 的 9000 TCP 端口。"
echo_color "yellow" "- 在浏览器中访问 http://${REMOTE_HOST}:${FRONTEND_PORT}"
echo_color "yellow" "- 检查服务器上的后端日志: ${SSH_CMD} 'tail -f ${REMOTE_BACKEND_DIR}/backend.log'"
echo_color "yellow" "- 检查服务器上的 Nginx 日志: /var/log/nginx/${NGINX_CONF_NAME}.access.log 和 .error.log"
echo_color "yellow" "- 确保服务器防火墙允许 TCP 端口 ${FRONTEND_PORT}, ${BACKEND_PORT}, 和 9000 的入站连接。"
